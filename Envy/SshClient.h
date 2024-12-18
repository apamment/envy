#pragma once

#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>
#include <string>
class SshClient {
public:
  struct ssh_channel_callbacks_struct ssh_cb;

  ssh_session p_ssh_session;
  int csock;
  int rsock;

  std::string username;
  std::string password;

  int term_width;
  int term_height;
  char *term_type;

  void run();
  SshClient();
  ~SshClient();
  bool do_auth();
  bool dis_flag;
  bool got_auth;
  bool got_shell;
  ssh_channel chan;
  ssh_event ev;

private:
  void do_run();
};
