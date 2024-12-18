#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <libssh/libssh.h>
#include "SshClient.h"
#include "Disconnect.h"
#include "Node.h"
#include "../Common/INIReader.h"


#define RSA_KEYLEN 4096
#define DSA_KEYLEN 1024
#define ECDSA_KEYLEN 521
#define ED25519_KEYLEN 256


int main(int argc, char **argv) {
  int node = 0;
  int sock = 0;
  bool telnet = false;
  bool ssh = false;
  int ret = 0;

  for (int i = 1; i < argc; i++) {
    if (strcasecmp(argv[i], "-N") == 0) {
      node = (int)strtoul(argv[i + 1], NULL, 10);
      i++;
      continue;
    } else if (strcasecmp(argv[i], "-S") == 0) {
      sock = (int)strtoul(argv[i + 1], NULL, 10);
      i++;
      continue;
    } else if (strcasecmp(argv[i], "-T") == 0) {
      telnet = true;
    } else if (strcasecmp(argv[i], "-SSH") == 0) {
      ssh = true;
    }
  }
  if (ssh == true) {
    INIReader inir("envy.ini");

    if (inir.ParseError() != 0) {
      close(sock);
      return -1;
    }

    std::string datapath = inir.Get("Paths", "Data Path", "data");
    std::filesystem::path ssh_dsa_key(datapath + "/ssh_host_dsa_key");
    std::filesystem::path ssh_rsa_key(datapath + "/ssh_host_rsa_key");
    std::filesystem::path ssh_ecdsa_key(datapath + "/ssh_host_ecdsa_key");
    std::filesystem::path ssh_ed25519_key(datapath + "/ssh_host_ed25519_key");

    ssh_bind p_ssh_bind = NULL;

    if (!std::filesystem::exists(ssh_dsa_key)) {
      ssh_key new_key;
      int status = ssh_pki_generate(SSH_KEYTYPE_DSS, DSA_KEYLEN, &new_key);
      if (status == SSH_OK) {
        ssh_pki_export_privkey_file(new_key, NULL, NULL, NULL, ssh_dsa_key.u8string().c_str());
      }
    }

    if (!std::filesystem::exists(ssh_rsa_key)) {
      ssh_key new_key;
      int status = ssh_pki_generate(SSH_KEYTYPE_RSA, RSA_KEYLEN, &new_key);
      if (status == SSH_OK) {
        ssh_pki_export_privkey_file(new_key, NULL, NULL, NULL, ssh_rsa_key.u8string().c_str());
      }
    }
    if (!std::filesystem::exists(ssh_ecdsa_key)) {
      ssh_key new_key;
      int status = ssh_pki_generate(SSH_KEYTYPE_ECDSA, ECDSA_KEYLEN, &new_key);
      if (status == SSH_OK) {
        ssh_pki_export_privkey_file(new_key, NULL, NULL, NULL, ssh_ecdsa_key.u8string().c_str());
      }
    }
    if (!std::filesystem::exists(ssh_ed25519_key)) {
      ssh_key new_key;
      int status = ssh_pki_generate(SSH_KEYTYPE_ED25519, ED25519_KEYLEN, &new_key);
      if (status == SSH_OK) {
        ssh_pki_export_privkey_file(new_key, NULL, NULL, NULL, ssh_ed25519_key.u8string().c_str());
      }
    }

    if (std::filesystem::exists(ssh_dsa_key) || std::filesystem::exists(ssh_rsa_key) || std::filesystem::exists(ssh_ecdsa_key) || std::filesystem::exists(ssh_ed25519_key)) {
      p_ssh_bind = ssh_bind_new();
      if (p_ssh_bind != NULL) {

        if (std::filesystem::exists(ssh_dsa_key)) {
          ssh_bind_options_set(p_ssh_bind, SSH_BIND_OPTIONS_HOSTKEY, ssh_dsa_key.u8string().c_str());
        }

        if (std::filesystem::exists(ssh_rsa_key)) {
          ssh_bind_options_set(p_ssh_bind, SSH_BIND_OPTIONS_HOSTKEY, ssh_rsa_key.u8string().c_str());
        }

        if (std::filesystem::exists(ssh_ecdsa_key)) {
          ssh_bind_options_set(p_ssh_bind, SSH_BIND_OPTIONS_HOSTKEY, ssh_ecdsa_key.u8string().c_str());
        }
        if (std::filesystem::exists(ssh_ed25519_key)) {
          ssh_bind_options_set(p_ssh_bind, SSH_BIND_OPTIONS_HOSTKEY, ssh_ed25519_key.u8string().c_str());
        }

        // do ssh authentication
        SshClient *sshc = new SshClient();

        sshc->p_ssh_session = ssh_new();
        if (sshc->p_ssh_session == NULL) {
#ifdef _MSC_VER
          SetConsoleMode(hInput, in_prev_mode);
          closesocket(sock);
#else
          close(sock);
#endif
          delete sshc;

          return -1;
        } else {
          long timeout = 120;
          ssh_options_set(sshc->p_ssh_session, SSH_OPTIONS_TIMEOUT, (void *)&timeout);

          if (ssh_bind_accept_fd(p_ssh_bind, sshc->p_ssh_session, sock) == SSH_OK) {

            sockaddr_in sa;
#ifdef _MSC_VER
            int addr_len = sizeof(sockaddr_in);
#else
            socklen_t addr_len = sizeof(sockaddr_in);
#endif
            memset(&sa, 0, sizeof(sockaddr_in));

            sa.sin_family = AF_INET;
            sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            sa.sin_port = 0;
            if (!sshc->do_auth()) {
#ifdef _MSC_VER
              SetConsoleMode(hInput, in_prev_mode);
              closesocket(sock);
#else
              close(sock);
#endif
              delete sshc;
              return -1;
            }

            int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            if (bind(listener, (sockaddr *)&sa, sizeof(sockaddr_in)) < 0) {
#ifdef _MSC_VER
              SetConsoleMode(hInput, in_prev_mode);
              closesocket(sock);
#else
              close(sock);
#endif
              delete sshc;
              return -1;
            }

            if (listen(listener, 2) < 0) {
#ifdef _MSC_VER
              SetConsoleMode(hInput, in_prev_mode);
              closesocket(sock);
#else
              close(sock);
#endif
              delete sshc;
              return -1;
            }

            if (getsockname(listener, (sockaddr *)&sa, &addr_len) < 0) {
#ifdef _MSC_VER
              SetConsoleMode(hInput, in_prev_mode);
              closesocket(sock);
#else
              close(sock);
#endif
              delete sshc;
              return -1;
            }
            int rsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

            int new_sock = -1;

            sshc->rsock = rsock;
            sshc->csock = sock;

            std::thread t([&sshc, &sa, addr_len]() {
              if (connect(sshc->rsock, (sockaddr *)&sa, addr_len) < 0) {
#ifdef _MSC_VER
                closesocket(sshc->csock);
#else
                close(sshc->csock);
#endif
                return;
              }
              sshc->run();
            });
            // t.detach();
            new_sock = accept(listener, (sockaddr *)&sa, &addr_len);
#ifdef _MSC_VER
            closesocket(listener);
#else
            close(listener);
#endif
            if (new_sock == -1) {
              return -1;
            }
            Node n(node, new_sock, false);
            n.set_term_width(sshc->term_width);
            n.set_term_height(sshc->term_height);
            n.sshc = sshc;
            n.ssht = &t;
            try {
              ret = n.run(&sshc->username, &sshc->password);
              sshc->dis_flag = true;
#ifdef _MSC_VER
              closesocket(new_sock);
#else
              close(new_sock);
#endif
            } catch (DisconnectException e) {
              sshc->dis_flag = true;
              ret = -1;
            }
            t.join();
          }
        }
        delete sshc;
      }
    }
  }

  if (telnet && sock != 0 && node != 0) {
    Node n(node, sock, telnet);

    try {
      ret = n.run();
    } catch (DisconnectException e) {
      ret = -1;
    }
  }

  return ret;
}
