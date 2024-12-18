#include <sys/wait.h>
#include <unistd.h>
#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__APPLE__)
#include <libgen.h>
#include <termios.h>
#if defined(__FreeBSD__)
#include <libutil.h>
#else
#include <util.h>
#endif
#include <sys/ioctl.h>
#else
#include <pty.h>
#endif
#include <signal.h>
#include "Door.h"
#include "Node.h"
#include "User.h"
#include "Version.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/socket.h>

#define _stricmp strcasecmp
#define LINE_END "\r\n"

int running_door;
pid_t door_pid;

void doorchld_handler(int s) {
  int tmperrno = errno;

  pid_t p;

  while ((p = waitpid(-1, NULL, WNOHANG)) > 0) {
    if (door_pid == p) {
      running_door = 0;
    }
  }

  errno = tmperrno;
}

int ttySetRaw(int fd, struct termios *prevTermios) {
  struct termios t;

  if (tcgetattr(fd, &t) == -1)
    return -1;

  if (prevTermios != NULL)
    *prevTermios = t;

  t.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
  t.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR | INPCK | ISTRIP | IXON | PARMRK);
  t.c_oflag &= ~OPOST;
  t.c_cc[VMIN] = 1;
  t.c_cc[VTIME] = 0;

  if (tcsetattr(fd, TCSAFLUSH, &t) == -1)
    return -1;

  return 0;
}

void Door::createDropfiles(Node *n) {
  std::filesystem::path fpath;
  fpath.append(n->get_tmp_path());
  fpath.append(std::to_string(n->get_node()));
  if (!std::filesystem::exists(fpath)) {
    std::filesystem::create_directories(fpath);
  }

  std::filesystem::path dorinfo(fpath);
  dorinfo.append("dorinfo1.def");

  std::ofstream f4(dorinfo);

  f4 << n->get_bbsname() << LINE_END;
  f4 << n->get_opname() << LINE_END;
  f4 << " " << LINE_END;
  f4 << "COM1" << LINE_END;
  f4 << "115200 BAUD,N,8,1" << LINE_END;
  f4 << "0" << LINE_END;
  std::string s = User::get_attrib(n, "fullname", "UNKNOWN");

  if (s.find(' ') != std::string::npos) {
    f4 << s.substr(0, s.find(' ')) << LINE_END;
    if (s.find(' ') < s.length()) {
      f4 << s.substr(s.find(' ') + 1) << LINE_END;
    } else {
      f4 << "Unknown";
    }
  } else {
    f4 << "Unknown" << LINE_END;
    f4 << "Unknown" << LINE_END;
  }

  f4 << User::get_attrib(n, "location", "Somewhere, The World") << LINE_END;
  f4 << "1" << LINE_END;
  f4 << n->get_seclevel() << LINE_END;                 // sec level
  f4 << std::to_string(n->get_timeleft()) << LINE_END; // time left
  f4 << "-1" << LINE_END;
  f4.close();

  std::filesystem::path chaintxt(fpath);
  chaintxt.append("chain.txt");

  std::ofstream f3(chaintxt);

  f3 << n->get_uid() << LINE_END;
  f3 << n->get_username() << LINE_END;
  f3 << User::get_attrib(n, "fullname", "UNKNOWN") << LINE_END;
  f3 << "NONE" << LINE_END;
  f3 << "21" << LINE_END;
  f3 << "M" << LINE_END;
  f3 << "0" << LINE_END;
  f3 << "01/01/71" << LINE_END;
  f3 << "80" << LINE_END;
  f3 << "25" << LINE_END;
  f3 << n->get_seclevel() << LINE_END;
  f3 << "0" << LINE_END;
  f3 << "0" << LINE_END;
  f3 << "1" << LINE_END;
  f3 << "1" << LINE_END;
  f3 << std::to_string(n->get_timeleft() * 60) << LINE_END; // time left
  f3 << n->get_gfile_path() << LINE_END;
  f3 << n->get_tmp_path() << LINE_END;
  f3 << "NOLOG" << LINE_END;
  f3 << "9600" << LINE_END;
  f3 << "1" << LINE_END;
  f3 << n->get_bbsname() << LINE_END;
  f3 << n->get_opname() << LINE_END;
  f3 << "0" << LINE_END;
  f3 << "0" << LINE_END;
  f3 << "0" << LINE_END;
  f3 << "0" << LINE_END;
  f3 << "0" << LINE_END;
  f3 << "0" << LINE_END;
  f3 << "8N1" << LINE_END;
  f3.close();

  std::filesystem::path d32path(fpath);

  d32path.append("door32.sys");

  std::ofstream f(d32path);
  f << "0" << LINE_END;
  f << n->get_socket() << LINE_END;
  f << "38400" << LINE_END;
  f << "Envy " << VERSION << "-" << GITV << LINE_END;
  f << n->get_uid() << LINE_END;
  f << User::get_attrib(n, "fullname", "UNKNOWN") << LINE_END;
  f << n->get_username() << LINE_END;
  f << n->get_seclevel() << LINE_END;                 // seclevel
  f << std::to_string(n->get_timeleft()) << LINE_END; // time left
  f << (n->has_ansi() ? "1" : "0") << LINE_END;
  f << n->get_node() << LINE_END;

  f.close();

  std::filesystem::path doorsyspath(fpath);
  doorsyspath.append("door.sys");

  std::ofstream f2(doorsyspath);

  f2 << "COM1:" << LINE_END;
  f2 << "38400" << LINE_END;
  f2 << "8" << LINE_END;
  f2 << n->get_node() << LINE_END;
  f2 << "38400" << LINE_END;
  f2 << "Y" << LINE_END;
  f2 << "N" << LINE_END;
  f2 << "Y" << LINE_END;
  f2 << "Y" << LINE_END;
  f2 << User::get_attrib(n, "fullname", "UNKNOWN") << LINE_END;
  f2 << User::get_attrib(n, "location", "Somewhere, The World") << LINE_END;
  f2 << "00-0000-0000" << LINE_END;
  f2 << "00-0000-0000" << LINE_END;
  f2 << "SECRET" << LINE_END;
  f2 << n->get_seclevel() << LINE_END; // sec level
  f2 << User::get_attrib(n, "total-calls", "0") << LINE_END;
  f2 << "01-01-1971" << LINE_END;
  f2 << std::to_string(n->get_timeleft() * 60) << LINE_END;
  f2 << n->get_timeleft() << LINE_END;
  f2 << "GR" << LINE_END;
  f2 << "25" << LINE_END;
  f2 << "N" << LINE_END;
  f2 << LINE_END;
  f2 << LINE_END;
  f2 << LINE_END;
  f2 << n->get_uid() << LINE_END;
  f2 << LINE_END;
  f2 << "0" << LINE_END;
  f2 << "0" << LINE_END;
  f2 << "0" << LINE_END;
  f2 << "99999" << LINE_END;
  f2 << "01-01-1971" << LINE_END;
  f2 << LINE_END;
  f2 << LINE_END;
  f2 << n->get_opname() << LINE_END;
  f2 << n->get_username() << LINE_END;
  f2 << "none" << LINE_END;
  f2 << "Y" << LINE_END;
  f2 << "N" << LINE_END;
  f2 << "Y" << LINE_END;
  f2 << "7" << LINE_END;
  f2 << "0" << LINE_END;
  f2 << "01-01-1971" << LINE_END;
  f2 << "00:00" << LINE_END;
  f2 << "00:00" << LINE_END;
  f2 << "32768" << LINE_END;
  f2 << "0" << LINE_END;
  f2 << "0" << LINE_END;
  f2 << "0" << LINE_END;
  f2 << "None." << LINE_END;
  f2 << "0" << LINE_END;
  f2 << "0" << LINE_END;

  f2.close();
}

bool telnet_bin_mode;

bool Door::runExternal(Node *n, std::string command, std::vector<std::string> args, bool raw) {
  pid_t pid;
  char **argv;
  int door_in;
  int door_out;
  struct winsize ws;
  struct sigaction sa;
  struct sigaction osa;
  int t;
  fd_set fdset;
  int master;
  int slave;
  struct timeval thetimeout;
  int ret;
  int len;
  unsigned char inbuf[256];
  unsigned char outbuf[512];
  int gotiac;
  int g;
  int h;
  unsigned char c;
  struct termios oldit2;
  int iac;

  unsigned char iac_binary_will[] = {IAC, IAC_WILL, IAC_TRANSMIT_BINARY, '\0'};
  unsigned char iac_binary_wont[] = {IAC, IAC_WONT, IAC_TRANSMIT_BINARY, '\0'};
  unsigned char iac_binary_do[] = {IAC, IAC_DO, IAC_TRANSMIT_BINARY, '\0'};
  unsigned char iac_binary_dont[] = {IAC, IAC_DONT, IAC_TRANSMIT_BINARY, '\0'};

  door_in = n->get_socket();
  door_out = n->get_socket();

  argv = (char **)malloc(sizeof(char *) * (args.size() + 2));
  if (!argv) {
    return true;
  }

  argv[0] = strdup(command.c_str());
  for (size_t i = 0; i < args.size(); i++) {
    argv[i + 1] = strdup(args.at(i).c_str());
  }
  argv[args.size() + 1] = NULL;

  ws.ws_row = 24;
  ws.ws_col = 80;
  running_door = 1;

  if (openpty(&master, &slave, NULL, NULL, &ws) == 0) {
    sa.sa_handler = doorchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    if (sigaction(SIGCHLD, &sa, &osa) == -1) {
      for (size_t j = 0; j < args.size(); j++) {
        free(argv[j]);
      }
      free(argv);
      perror("sigaction");
      return true;
    }

    ttySetRaw(master, &oldit2);
    ttySetRaw(slave, &oldit2);

    pid = fork();

    if (pid < 0) {
      n->bprintf("\r\nFailed to run door\r\n");
      for (size_t j = 0; j < args.size(); j++) {
        free(argv[j]);
      }
      free(argv);
      sigaction(SIGCHLD, &osa, NULL);
      return true;
    } else if (pid == 0) {
      close(master);
      dup2(slave, 0);
      dup2(slave, 1);

      close(slave);
      setsid();
      ioctl(0, TIOCSCTTY, 1);
      execvp(command.c_str(), argv);
      exit(0);
    } else {
      door_pid = pid;
      gotiac = 0;
      while (running_door) {
        if (door_in == -1) {
          // ssh client disconnected, closed the socket on us.
          for (size_t j = 0; j < args.size(); j++) {
            free(argv[j]);
          }
          free(argv);
          return false;
        }

        FD_ZERO(&fdset);
        FD_SET(master, &fdset);
        FD_SET(door_in, &fdset);

        if (master > door_in) {
          t = master + 1;
        } else {
          t = door_in + 1;
        }

        thetimeout.tv_sec = 5;
        thetimeout.tv_usec = 0;

        ret = select(t, &fdset, NULL, NULL, &thetimeout);
        if (ret > 0) {
          if (FD_ISSET(door_in, &fdset)) {
            len = read(door_in, inbuf, 256);
            if (len == 0) {
              close(master);
              for (size_t i = 0; i < args.size() + 1; i++) {
                free(argv[i]);
              }
              free(argv);
              sigaction(SIGCHLD, &osa, NULL);
              return false;
            }
            g = 0;
            for (h = 0; h < len; h++) {
              c = inbuf[h];
              if (!raw) {
                if (c == '\n' || c == '\0') {
                  continue;
                }
              }

              if (c == 255 && n->is_telnet()) {
                if (gotiac == 1) {
                  outbuf[g++] = c;
                  gotiac = 0;
                } else {
                  gotiac = 1;
                }
              } else {
                if (gotiac == 1) {
                  if (c == 254 || c == 253 || c == 252 || c == 251) {
                    iac = c;
                    gotiac = 2;
                  } else if (c == 250) {
                    gotiac = 3;
                  } else {
                    gotiac = 0;
                  }
                } else if (gotiac == 2) {
                  if (c == IAC_TRANSMIT_BINARY) {
                    if (iac == IAC_DO) {
                      if (!telnet_bin_mode) {
                        write(master, iac_binary_will, 3);
                        telnet_bin_mode = true;
                      }
                    } else if (iac == IAC_DONT) {
                      if (telnet_bin_mode) {
                        write(master, iac_binary_wont, 3);
                        telnet_bin_mode = false;
                      }
                    } else if (iac == IAC_WILL) {
                      if (!telnet_bin_mode) {
                        write(master, iac_binary_do, 3);
                        telnet_bin_mode = true;
                      }
                    } else if (iac == IAC_WONT) {
                      if (telnet_bin_mode) {
                        write(master, iac_binary_dont, 3);
                        telnet_bin_mode = false;
                      }
                    }
                  }
                  gotiac = 0;
                } else if (gotiac == 3) {
                  if (c == 240) {
                    gotiac = 0;
                  }
                } else {
                  outbuf[g++] = c;
                }
              }
            }

            write(master, outbuf, g);
          } else if (FD_ISSET(master, &fdset)) {
            len = read(master, inbuf, 256);
            if (len == 0) {
              close(master);
              break;
            }

            g = 0;
            for (h = 0; h < len; h++) {
              c = inbuf[h];
              if (c == 255 && n->is_telnet()) {
                outbuf[g++] = c;
              }
              outbuf[g++] = c;
            }
            if (raw) {
              write(door_out, outbuf, g);
            } else {
              send(n->get_socket(), (const char *)outbuf, g, 0);
            }
          }
        } else {
          if (ret == -1) {
            if (errno != EINTR) {
              sigaction(SIGCHLD, &osa, NULL);
              return false;
            }
          }
        }
      }
    }
  }

  for (size_t i = 0; i < args.size() + 1; i++) {
    free(argv[i]);
  }
  free(argv);
  sigaction(SIGCHLD, &osa, NULL);
  return true;
}

Door::Door() {}

Door::~Door() {}
