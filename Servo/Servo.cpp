#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Psapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#define strcasecmp _stricmp
#else
#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <libproc.h>
#endif
#endif
#include "../Common/INIReader.h"
#include "EventMgr.h"
#include "IPBlockItem.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

static time_t span;
static int count;

#ifndef _MSC_VER
void sigchld_handler(int s) {
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;
  pid_t p;
  while ((p = waitpid(-1, NULL, WNOHANG)) > 0) {

  }

  errno = saved_errno;
}
#endif

struct node_t {
  unsigned long pid;
  std::string ip;
};

std::vector<IPBlockItem *> *blocklist;
std::string datapath;

std::string ts() {
  time_t now = time(NULL);
  struct tm now_tm;
  char buffer[22];

#ifdef _MSC_VER
  localtime_s(&now_tm, &now);
#else
  localtime_r(&now, &now_tm);
#endif
  snprintf(buffer, sizeof buffer, "%04d/%02d/%02d %02d:%02d:%02d: ", now_tm.tm_year + 1900, now_tm.tm_mon + 1, now_tm.tm_mday, now_tm.tm_hour, now_tm.tm_min,
           now_tm.tm_sec);

  return std::string(buffer);
}

std::string err() {
#ifdef _MSC_VER
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole, 12);
  return "";
#else
  return "\x1b[1;31m";
#endif
}

std::string norm() {
#ifdef _MSC_VER
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole, 15);
  return "";
#else
  return "\x1b[1;37m";
#endif
}

std::string rst() {
#ifdef _MSC_VER
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleTextAttribute(hConsole, 7);
  return "";
#else
  return "\x1b[0m";
#endif
}

bool should_pass(std::string ip) {
  for (size_t i = 0; i < blocklist->size(); i++) {
    if (ip == blocklist->at(i)->getip()) {
      return blocklist->at(i)->should_pass();
    }
  }

  IPBlockItem *blockitem = new IPBlockItem(ip, datapath, false, false, span, count);
  blocklist->push_back(blockitem);
  return true;
}

bool in_multiallowed(std::vector<std::string> *list, std::string item) {
  for (size_t i = 0; i < list->size(); i++) {
    if (list->at(i) == item) {
      return true;
    }
  }

  return false;
}

int main() {
  bool ipv6 = false;
  int port;
  struct sockaddr_in  serv_addr, client_addr;
  struct sockaddr_in6 serv_addr6, client_addr6;
  int csockfd;
  int on = 1;
  size_t max_nodes = 4;

  char str[INET6_ADDRSTRLEN];
  std::vector<struct node_t> nodes;
  std::vector<std::string> multiallowed;
#ifdef _MSC_VER
  WSADATA wsaData;

  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    std::cerr << err() << "Error initializing winsock!" << rst() << std::endl;
    return -1;
  }
#else
  struct sigaction sa;
  char sockstr[10];
  char nodestr[10];
  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_SIGINFO;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction - sigchld");
    exit(1);
  }

#endif
  INIReader inir("envy.ini");
  if (inir.ParseError() != 0) {
    return -1;
  }

  port = inir.GetInteger("main", "telnet port", 2323);
  max_nodes = inir.GetInteger("main", "max nodes", 4);
  datapath = inir.Get("paths", "data path", "data");
  ipv6 = inir.GetBoolean("main", "enable ipv6", false);
  span = inir.GetInteger("main", "ip block timeout", 300);
  count = inir.GetInteger("main", "ip block attempts", 5);

  EventMgr ev;

  ev.run(datapath);

  blocklist = new std::vector<IPBlockItem *>();

  std::ifstream passlistf(datapath + "/passlist.ip");
  std::string line;

  while (std::getline(passlistf, line)) {
    IPBlockItem *item = new IPBlockItem(line, datapath, false, true, span, count);
    blocklist->push_back(item);
  }
  passlistf.close();

  std::ifstream blocklistf(datapath + "/blocklist.ip");
  while (std::getline(blocklistf, line)) {
    IPBlockItem *item = new IPBlockItem(line, datapath, true, false, span, count);
    blocklist->push_back(item);
  }
  blocklistf.close();

  std::ifstream multiallowf(datapath + "/multiallow.ip");
  while (std::getline(multiallowf, line)) {
    multiallowed.push_back(line);
  }
  multiallowf.close();

  for (size_t i = 0; i < max_nodes; i++) {
    struct node_t n;
    n.pid = 0;
    n.ip = "";

    nodes.push_back(n);
  }

  // listen on telnet

  int telnetfd = socket(AF_INET, SOCK_STREAM, 0);
  int telnetfd6 = -1;

  memset(&serv_addr, 0, sizeof(struct sockaddr_in));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);
  if (setsockopt(telnetfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
    std::cerr << err() << ts() << "NodeManager : Error setting SO_REUSEADDR (Telnet)" << rst() << std::endl;
    return -1;
  }
  if (setsockopt(telnetfd, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on)) < 0) {
    std::cerr << err() << ts() << "NodeManager : Error setting TCP_NODELAY (Telnet)" << rst() << std::endl;
    return -1;
  }
  if (bind(telnetfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr_in)) < 0) {
    std::cerr << err() << ts() << "NodeManager : Error binding. (Telnet)" << rst() << std::endl;
    return -1;
  }

  // listen on telnet6

  if (ipv6) {
    telnetfd6 = socket(AF_INET6, SOCK_STREAM, 0);
    memset(&serv_addr6, 0, sizeof(struct sockaddr_in6));

    serv_addr6.sin6_family = AF_INET6;
    serv_addr6.sin6_addr = in6addr_any;
    serv_addr6.sin6_port = htons(port);
    if (setsockopt(telnetfd6, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&on, sizeof(on)) < 0) {
      std::cerr << err() << ts() << "NodeManager : Error setting IPV6_V6ONLY (Telnet - ipv6)" << rst() << std::endl;
      return -1;
    }
    if (setsockopt(telnetfd6, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
      std::cerr << err() << ts() << "NodeManager : Error setting SO_REUSEADDR (Telnet - ipv6)" << rst() << std::endl;
      return -1;
    }
    if (setsockopt(telnetfd6, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on)) < 0) {
      std::cerr << err() << ts() << "NodeManager : Error setting TCP_NODELAY (Telnet - ipv6)" << rst() << std::endl;
      return -1;
    }
    if (bind(telnetfd6, (struct sockaddr *)&serv_addr6, sizeof(struct sockaddr_in6)) < 0) {
      std::cerr << err() << ts() << "NodeManager : Error binding. (Telnet - ipv6)" << rst() << std::endl;
      return -1;
    }

    listen(telnetfd6, 5);
    std::cout << norm() << ts() << "NodeManager : Listening on port " << port << "(TELNET - ipv6)" << rst() << std::endl;
  }

  listen(telnetfd, 5);
  std::cout << norm() << ts() << "NodeManager : Listening on port " << port << "(TELNET)" << rst() << std::endl;

  int sshfd = -1;
  int sshfd6 = -1;

    int nfds;
  int maxfd = telnetfd;
  fd_set server_fds;
  FD_ZERO(&server_fds);
  FD_SET(telnetfd, &server_fds);

  if (ipv6) {
    FD_SET(telnetfd6, &server_fds);
    if (telnetfd6 > maxfd)
      maxfd = telnetfd6;
  }

  nfds = maxfd;
  nfds++;

  while (1) {
    csockfd = -1;
    bool telnet = false;
    bool ipv6con = false;
    fd_set copy_fds = server_fds;

    int clen = sizeof(struct sockaddr_in);
    int clen6 = sizeof(struct sockaddr_in6);

    memset(&client_addr, 0, clen);

    if (select(nfds, &copy_fds, NULL, NULL, NULL) < 0) {
      if (errno == EINTR) {
        continue;
      } else {
        exit(-1);
      }
    }

    if (FD_ISSET(telnetfd, &copy_fds)) {
      csockfd = accept(telnetfd, (struct sockaddr *)&client_addr, (socklen_t *)&clen);
      telnet = true;
    }
    if (ipv6) {
      if (FD_ISSET(telnetfd6, &copy_fds)) {
        csockfd = accept(telnetfd6, (struct sockaddr *)&client_addr6, (socklen_t *)&clen6);
        telnet = true;
        ipv6con = true;
      }
    }
   if (csockfd != -1) {
      std::string ipaddr;
      if (!ipv6con) {
        ipaddr = std::string(inet_ntop(AF_INET, &((struct sockaddr_in *)&client_addr)->sin_addr, str, sizeof(str)));
      } else {
        ipaddr = std::string(inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&client_addr6)->sin6_addr, str, sizeof(str)));
      }
      if (!should_pass(ipaddr)) {
        std::cout << norm() << ts() << "NodeManager : Blocking ip " << ipaddr << " (Blocklist)" << rst() << std::endl;
#ifdef _MSC_VER
        closesocket(csockfd);
#else
        close(csockfd);
#endif
        continue;
      }

#ifdef _MSC_VER

      for (size_t i = 0; i < max_nodes; i++) {
        if (nodes.at(i).pid != 0) {
          HANDLE Handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, nodes.at(i).pid);

          if (Handle) {
            char pname[256];

            if (GetModuleBaseNameA(Handle, 0, pname, 256) != 0) {
              if (strcasecmp(pname, "envy.exe") == 0) {
                CloseHandle(Handle);
                continue;
              }
            }
            CloseHandle(Handle);
          }
          nodes.at(i).pid = 0;
          nodes.at(i).ip = "";
        }
      }

      bool alreadyloggedin = false;
      for (size_t n = 0; n < nodes.size(); n++) {
        if (nodes.at(n).ip == ipaddr) {
          alreadyloggedin = true;
          break;
        }
      }

      if (alreadyloggedin && !in_multiallowed(&multiallowed, ipaddr)) {
        std::cout << norm() << ts() << "NodeManager : Blocking ip " << ipaddr << " (Already logged in)" << rst() << std::endl;
        closesocket(csockfd);
        continue;
      }

      bool foundnode = false;

      for (size_t i = 0; i < max_nodes; i++) {
        if (nodes.at(i).pid == 0) {
          foundnode = true;
          std::stringstream ss;
          ss.str("");
          ss << "\"envy.exe\""
             << " -S " << csockfd << " -N " << std::to_string(i + 1);
          if (telnet) {
            std::cout << norm() << ts() << "NodeManager : Launching envy (Telnet - " << ipaddr << ")" << rst() << std::endl;
            ss << " -T";
          } else {
            std::cout << norm() << ts() << "NodeManager : Launching envy (SSH - " << ipaddr << ")" << rst() << std::endl;
            ss << " -SSH";
          }
          char *cmd = strdup(ss.str().c_str());

          STARTUPINFOA si;
          PROCESS_INFORMATION pi;

          ZeroMemory(&si, sizeof(si));
          si.cb = sizeof(si);
          si.dwFlags = STARTF_USESHOWWINDOW;
          si.wShowWindow = SW_MINIMIZE;
          //	si.dwFlags = STARTF_USESTDHANDLES;
          //	si.hStdInput = INVALID_HANDLE_VALUE;
          //	si.hStdError = INVALID_HANDLE_VALUE;
          //	si.hStdOutput = INVALID_HANDLE_VALUE;

          ZeroMemory(&pi, sizeof(pi));

          if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
            std::cerr << err() << ts() << "NodeManager : Failed to create process!" << rst() << std::endl;
            free(cmd);
            closesocket(csockfd);
            continue;
          }
          nodes.at(i).pid = pi.dwProcessId;
          nodes.at(i).ip = ipaddr;
          CloseHandle(pi.hProcess);
          CloseHandle(pi.hThread);
          free(cmd);
          break;
        }
      }
      if (!foundnode) {
        send(csockfd, "BUSY\r\n", 6, 0);
      }
      closesocket(csockfd);
#else

      for (size_t i = 0; i < max_nodes; i++) {
        if (nodes.at(i).pid != 0) {
#ifdef __APPLE__
	        struct proc_bsdshortinfo bsdinfo;
	        int ret;

	        ret = proc_pidinfo(nodes.at(i).pid, PROC_PIDT_SHORTBSDINFO, 0, &bsdinfo, sizeof bsdinfo);

	        if (ret == sizeof bsdinfo && strncmp(bsdinfo.pbsi_comm, "envy", 8) == 0) {
            continue;
	        }
#else
          char buffer[PATH_MAX];
          snprintf(buffer, sizeof buffer, "/proc/%lu/cmdline", nodes.at(i).pid);
          FILE *fptr = fopen(buffer, "r");

          if (fptr) {
            fgets(buffer, sizeof buffer, fptr);
            fclose(fptr);

            if (strncmp(buffer, "./envy", 10) == 0) {
              continue;
            }
          }
#endif
          nodes.at(i).pid = 0;
          nodes.at(i).ip = "";
        }
      }
      bool alreadyloggedin = false;
      for (size_t i = 0; i < nodes.size(); i++) {
        if (nodes.at(i).ip == ipaddr) {
          alreadyloggedin = true;
          break;
        }
      }

      if (alreadyloggedin && !in_multiallowed(&multiallowed, ipaddr)) {
        std::cout << norm() << ts() << "NodeManager : Blocking ip " << ipaddr << " (Already logged in)" << rst() << std::endl;
        close(csockfd);
        continue;
      }

      bool foundnode = false;
      for (size_t i = 0; i < max_nodes; i++) {
        if (nodes.at(i).pid == 0) {
          foundnode = true;
          pid_t pid = fork();

          if (pid > 0) {
            nodes.at(i).pid = pid;
            nodes.at(i).ip = ipaddr;
            close(csockfd);
          } else if (pid == 0) {
            close(telnetfd);
            snprintf(sockstr, 10, "%d", csockfd);
            snprintf(nodestr, 10, "%lu", i + 1);

            std::cout << norm() << ts() << "NodeManager : Launching envy (Telnet - " << ipaddr << ")" << rst() << std::endl;
            if (execlp("./envy", "./envy", "-S", sockstr, "-N", nodestr, "-T", NULL) == -1) {
              perror("Execlp: ");
              exit(-1);
            }
          } else {
            std::cerr << err() << ts() << "NodeManager : Failed to create process!" << rst() << std::endl;
            close(csockfd);
          }
          break;
        }
      }
      if (!foundnode) {
        std::cerr << err() << ts() << "NodeManager : All nodes busy." << rst() << std::endl;
        send(csockfd, "BUSY\r\n", 6, 0);
        close(csockfd);
      }
#endif
    }
  }
  return 0;
}