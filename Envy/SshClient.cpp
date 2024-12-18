
#ifndef _MSC_VER
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#endif
#include "SshClient.h"

SshClient::SshClient() {
  chan = NULL;
  dis_flag = false;
  got_auth = false;
  got_shell = false;
  ev = NULL;
}

SshClient::~SshClient() {}

static int ssh_copy_fd_to_chan(socket_t fd, int revents, void *userdata) {
  ssh_channel chan = (ssh_channel)userdata;
  char buf[2048];
  int sz = 0;

  if (!chan) {
    return -1;
  }

  if (revents & POLLIN) {
    sz = recv(fd, buf, 2048, 0);
    if (sz > 0) {
      ssh_channel_write(chan, buf, sz);
    } else {
      ssh_channel_close(chan);
      sz = -1;
    }
  }
  if (revents & (POLLHUP | POLLERR | POLLNVAL)) {
    ssh_channel_close(chan);
    sz = -1;
  }

  return sz;
}

static int ssh_copy_chan_to_fd(ssh_session session, ssh_channel channel, void *data, uint32_t len, int is_stderr, void *userdata) {
  SshClient *sshc = (SshClient *)userdata;
  int sz;
  (void)session;
  (void)channel;
  (void)is_stderr;

  sz = send(sshc->rsock, (const char *)data, len, 0);

  if (sshc->dis_flag) {
    ssh_channel_close(channel);
  }

  return sz;
}

static void ssh_chan_close(ssh_session session, ssh_channel channel, void *userdata) {
  (void)userdata;
  (void)session;
  (void)channel;
}

void SshClient::run() {
  do_run();

  if (rsock != -1) {

#ifdef _MSC_VER
    shutdown(rsock, SD_BOTH);
    closesocket(rsock);
#else
    shutdown(rsock, SHUT_RDWR);
    close(rsock);
#endif
  }
  ssh_disconnect(p_ssh_session);
  ssh_free(p_ssh_session);
#ifdef _MSC_VER
  closesocket(csock);
#else
  close(csock);
#endif
}

static ssh_channel channel_open(ssh_session session, void *userdata) {
  SshClient *c = (SshClient *)userdata;

  c->chan = ssh_channel_new(session);
  return c->chan;
}

static int auth_password(ssh_session session, const char *user, const char *pass, void *userdata) {
  SshClient *c = (SshClient *)userdata;

  (void)session;

  c->username = std::string(user);
  c->password = std::string(pass);

  c->got_auth = true;

  return SSH_AUTH_SUCCESS;
}

bool SshClient::do_auth() {
  ssh_message msg;
  struct ssh_server_callbacks_struct server_cb;

  memset(&server_cb, 0, sizeof(struct ssh_server_callbacks_struct));

  server_cb.userdata = this;
  server_cb.auth_password_function = auth_password;
  server_cb.channel_open_request_session_function = channel_open;


  ssh_set_auth_methods(p_ssh_session, SSH_AUTH_METHOD_PASSWORD);

  ssh_callbacks_init(&server_cb);
  ssh_set_server_callbacks(p_ssh_session, &server_cb);

  if (ssh_handle_key_exchange(p_ssh_session)) {
    return false;
  }

  ev = ssh_event_new();
  if (ev == NULL) {
    return false;
  }
  if (ssh_event_add_session(ev, p_ssh_session) != SSH_OK) {
    ssh_event_free(ev);

    return false;
  }

  int n = 200;

  while (!got_auth && n > 1) {
    if (ssh_event_dopoll(ev, 100) == SSH_ERROR) {
      ssh_event_remove_session(ev, p_ssh_session);
      ssh_event_free(ev);
      return false;
    }
    n--;
  }

  if (!got_auth) {
    ssh_event_remove_session(ev, p_ssh_session);
    ssh_event_free(ev);
    return false;  
  }

  n = 200;

  while (!chan && n > 1) {
    if (ssh_event_dopoll(ev, 100) == SSH_ERROR) {
      ssh_event_remove_session(ev, p_ssh_session);
      ssh_event_free(ev);
      return false;
    }
    n--;
  }

  if (!chan) {
      ssh_event_remove_session(ev, p_ssh_session);
      ssh_event_free(ev);
      return false;
  }
  
  return true;
}

static int pty_request(ssh_session session, ssh_channel channel, const char *term, int cols, int rows, int py, int px, void *userdata) {
  SshClient *c = (SshClient *)userdata;

  (void)session;
  (void)channel;
  (void)term;

  c->term_height = rows;
  c->term_width = cols;
  c->term_type = strdup(term);

  return SSH_OK;
}

static int shell_request(ssh_session session, ssh_channel channel, void *userdata) {
  SshClient *c = (SshClient *)userdata;

  (void)session;
  (void)channel;

  if (c->got_shell)
    return SSH_ERROR;

  c->got_shell = true;

  return SSH_OK;
}

static int pty_resize(ssh_session session, ssh_channel channel, int cols, int rows, int py, int px, void *userdata) {
  SshClient *c = (SshClient *)userdata;

  (void)session;
  (void)channel;
  c->term_height = rows;
  c->term_width = cols;

  return SSH_OK;
}

void SshClient::do_run() {
  if (rsock == -1) {
    return;
  }

  memset(&ssh_cb, 0, sizeof(struct ssh_channel_callbacks_struct));
  ssh_cb.channel_pty_request_function = pty_request;
  ssh_cb.channel_pty_window_change_function = pty_resize;
  ssh_cb.channel_shell_request_function = shell_request;
  ssh_cb.channel_data_function = ssh_copy_chan_to_fd;
  ssh_cb.channel_eof_function = ssh_chan_close;
  ssh_cb.channel_close_function = ssh_chan_close;
  ssh_cb.userdata = this;

  ssh_callbacks_init(&ssh_cb);
  ssh_set_channel_callbacks(chan, &ssh_cb);

  short events = POLLIN | POLLERR | POLLHUP | POLLNVAL;

  int n = 200;

  while (!got_shell && n > 1) {
    if (ssh_event_dopoll(ev, 100) == SSH_ERROR) {
      ssh_event_free(ev);
      return;
    }
    n--;
  }

  if (!got_shell) {
    ssh_event_free(ev);
    return;  
  }

  if (ssh_event_add_fd(ev, rsock, events, ssh_copy_fd_to_chan, chan) != SSH_OK) {
    return;
  }

  do {
    ssh_event_dopoll(ev, 1000);
  } while (!ssh_channel_is_closed(chan));

  ssh_event_remove_fd(ev, rsock);

  ssh_event_remove_session(ev, p_ssh_session);

  ssh_event_free(ev);
}
