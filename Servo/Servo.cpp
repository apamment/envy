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
  int sshport;
  int gopherport;
  int nntpport;
  int binkport;
  int httpport;
  std::string httproot;
  bool ipv6 = false;
  int port;
  struct sockaddr_in nntp_serv_addr, gopher_serv_addr, ssh_serv_addr, serv_addr, client_addr, bink_serv_addr;
  struct sockaddr_in6 nntp_serv_addr6, gopher_serv_addr6, ssh_serv_addr6, serv_addr6, client_addr6, bink_serv_addr6;
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
  sshport = inir.GetInteger("main", "ssh port", -1);
  max_nodes = inir.GetInteger("main", "max nodes", 4);
  gopherport = inir.GetInteger("main", "gopher port", -1);
  nntpport = inir.GetInteger("main", "nntp port", -1);
  binkport = inir.GetInteger("main", "binkp port", -1);
  httpport = inir.GetInteger("main", "http port", -1);
  httproot = inir.Get("paths", "http root", "");
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

  if (sshport != -1) {
    sshfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&ssh_serv_addr, 0, sizeof(struct sockaddr_in));

    ssh_serv_addr.sin_family = AF_INET;
    ssh_serv_addr.sin_addr.s_addr = INADDR_ANY;
    ssh_serv_addr.sin_port = htons(sshport);
    if (setsockopt(sshfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
      std::cerr << err() << ts() << "NodeManager : Error setting SO_REUSEADDR (SSH)" << rst() << std::endl;
      return -1;
    }
    if (setsockopt(sshfd, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on)) < 0) {
      std::cerr << err() << ts() << "NodeManager : Error setting TCP_NODELAY (SSH)" << rst() << std::endl;
      return -1;
    }
    if (bind(sshfd, (struct sockaddr *)&ssh_serv_addr, sizeof(struct sockaddr_in)) < 0) {
      std::cerr << err() << ts() << "NodeManager : Error binding. (SSH)" << rst() << std::endl;
      return -1;
    }

    if (ipv6) {
      sshfd6 = socket(AF_INET6, SOCK_STREAM, 0);

      memset(&ssh_serv_addr6, 0, sizeof(struct sockaddr_in6));

      ssh_serv_addr6.sin6_family = AF_INET6;
      ssh_serv_addr6.sin6_addr = in6addr_any;
      ssh_serv_addr6.sin6_port = htons(sshport);
      if (setsockopt(sshfd6, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&on, sizeof(on)) < 0) {
        std::cerr << err() << ts() << "NodeManager : Error setting IPV6_V6ONLY (SSH - ipv6)" << rst() << std::endl;
        return -1;
      }
      if (setsockopt(sshfd6, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
        std::cerr << err() << ts() << "NodeManager : Error setting SO_REUSEADDR (SSH - ipv6)" << rst() << std::endl;
        return -1;
      }
      if (setsockopt(sshfd6, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on)) < 0) {
        std::cerr << err() << ts() << "NodeManager : Error setting TCP_NODELAY (SSH - ipv6)" << rst() << std::endl;
        return -1;
      }
      if (bind(sshfd6, (struct sockaddr *)&ssh_serv_addr6, sizeof(struct sockaddr_in6)) < 0) {
        std::cerr << err() << ts() << "NodeManager : Error binding. (SSH - ipv6)" << rst() << std::endl;
        return -1;
      }

      listen(sshfd6, 5);
      std::cout << norm() << ts() << "NodeManager : Listening on port " << sshport << "(SSH - ipv6)" << rst() << std::endl;
    }
    listen(sshfd, 5);
    std::cout << norm() << ts() << "NodeManager : Listening on port " << sshport << "(SSH)" << rst() << std::endl;
  }

  int gopherfd = -1;
  int gopherfd6 = -1;

  if (gopherport != -1) {
    gopherfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&gopher_serv_addr, 0, sizeof(struct sockaddr_in));

    gopher_serv_addr.sin_family = AF_INET;
    gopher_serv_addr.sin_addr.s_addr = INADDR_ANY;
    gopher_serv_addr.sin_port = htons(gopherport);
    if (setsockopt(gopherfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
      std::cerr << err() << ts() << "GopherServer: Error setting SO_REUSEADDR (Gopher)" << rst() << std::endl;
      return -1;
    }
    if (setsockopt(gopherfd, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on)) < 0) {
      std::cerr << err() << ts() << "GopherServer: Error setting TCP_NODELAY (Gopher)" << rst() << std::endl;
      return -1;
    }
    if (bind(gopherfd, (struct sockaddr *)&gopher_serv_addr, sizeof(struct sockaddr_in)) < 0) {
      std::cerr << err() << ts() << "GopherServer: Error binding. (Gopher)" << rst() << std::endl;
      return -1;
    }

    if (ipv6) {
      gopherfd6 = socket(AF_INET6, SOCK_STREAM, 0);

      memset(&gopher_serv_addr6, 0, sizeof(struct sockaddr_in6));

      gopher_serv_addr6.sin6_family = AF_INET6;
      gopher_serv_addr6.sin6_addr = in6addr_any;
      gopher_serv_addr6.sin6_port = htons(gopherport);

      if (setsockopt(gopherfd6, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&on, sizeof(on)) < 0) {
        std::cerr << err() << ts() << "GopherServer: Error setting IPV6_V6ONLY (Gopher - ipv6)" << rst() << std::endl;
        return -1;
      }
      if (setsockopt(gopherfd6, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
        std::cerr << err() << ts() << "GopherServer: Error setting SO_REUSEADDR (Gopher - ipv6)" << rst() << std::endl;
        return -1;
      }
      if (setsockopt(gopherfd6, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on)) < 0) {
        std::cerr << err() << ts() << "GopherServer: Error setting TCP_NODELAY (Gopher - ipv6)" << rst() << std::endl;
        return -1;
      }
      if (bind(gopherfd6, (struct sockaddr *)&gopher_serv_addr6, sizeof(struct sockaddr_in6)) < 0) {
        std::cerr << err() << ts() << "GopherServer: Error binding. (Gopher - ipv6)" << rst() << std::endl;
        return -1;
      }

      listen(gopherfd6, 5);
      std::cout << norm() << ts() << "GopherServer: Listening on port " << gopherport << "(Gopher - ipv6)" << rst() << std::endl;
    }
    listen(gopherfd, 5);
    std::cout << norm() << ts() << "GopherServer: Listening on port " << gopherport << "(Gopher)" << rst() << std::endl;
  }


  int nntpfd = -1;
  int nntpfd6 = -1;

  if (nntpport != -1) {
    nntpfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&nntp_serv_addr, 0, sizeof(struct sockaddr_in));

    nntp_serv_addr.sin_family = AF_INET;
    nntp_serv_addr.sin_addr.s_addr = INADDR_ANY;
    nntp_serv_addr.sin_port = htons(nntpport);
    if (setsockopt(nntpfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
      std::cerr << err() << ts() << "NNTPServer  : Error setting SO_REUSEADDR (NNTP)" << rst() << std::endl;
      return -1;
    }
    if (setsockopt(nntpfd, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on)) < 0) {
      std::cerr << err() << ts() << "NNTPServer  : Error setting TCP_NODELAY (NNTP)" << rst() << std::endl;
      return -1;
    }
    if (bind(nntpfd, (struct sockaddr *)&nntp_serv_addr, sizeof(struct sockaddr_in)) < 0) {
      std::cerr << err() << ts() << "NNTPServer  : Error binding. (NNTP)" << rst() << std::endl;
      return -1;
    }

    if (ipv6) {
      nntpfd6 = socket(AF_INET6, SOCK_STREAM, 0);

      memset(&nntp_serv_addr6, 0, sizeof(struct sockaddr_in6));

      nntp_serv_addr6.sin6_family = AF_INET6;
      nntp_serv_addr6.sin6_addr = in6addr_any;
      nntp_serv_addr6.sin6_port = htons(nntpport);

      if (setsockopt(nntpfd6, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&on, sizeof(on)) < 0) {
        std::cerr << err() << ts() << "NNTPServer  : Error setting IPV6_V6ONLY (NNTP - ipv6)" << rst() << std::endl;
        return -1;
      }
      if (setsockopt(nntpfd6, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
        std::cerr << err() << ts() << "NNTPServer  : Error setting SO_REUSEADDR (NNTP - ipv6)" << rst() << std::endl;
        return -1;
      }
      if (setsockopt(nntpfd6, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on)) < 0) {
        std::cerr << err() << ts() << "NNTPServer  : Error setting TCP_NODELAY (NNTP - ipv6)" << rst() << std::endl;
        return -1;
      }
      if (bind(nntpfd6, (struct sockaddr *)&nntp_serv_addr6, sizeof(struct sockaddr_in6)) < 0) {
        std::cerr << err() << ts() << "NNTPServer  : Error binding. (NNTP - ipv6)" << rst() << std::endl;
        return -1;
      }

      listen(nntpfd6, 5);
      std::cout << norm() << ts() << "NNTPServer  : Listening on port " << nntpport << "(NNTP - ipv6)" << rst() << std::endl;
    }
    listen(nntpfd, 5);
    std::cout << norm() << ts() << "NNTPServer  : Listening on port " << nntpport << "(NNTP)" << rst() << std::endl;
  }


  int binkfd = -1;
  int binkfd6 = -1;

  if (binkport != -1) {
    binkfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&bink_serv_addr, 0, sizeof(struct sockaddr_in));

    bink_serv_addr.sin_family = AF_INET;
    bink_serv_addr.sin_addr.s_addr = INADDR_ANY;
    bink_serv_addr.sin_port = htons(binkport);
    if (setsockopt(binkfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
      std::cerr << err() << ts() << "BinkpServer : Error setting SO_REUSEADDR (Binkp)" << rst() << std::endl;
      return -1;
    }
    if (setsockopt(binkfd, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on)) < 0) {
      std::cerr << err() << ts() << "BinkpServer : Error setting TCP_NODELAY (Binkp)" << rst() << std::endl;
      return -1;
    }
    if (bind(binkfd, (struct sockaddr *)&bink_serv_addr, sizeof(struct sockaddr_in)) < 0) {
      std::cerr << err() << ts() << "BinkpServer : Error binding. (Binkp)" << rst() << std::endl;
      return -1;
    }

    if (ipv6) {
      binkfd6 = socket(AF_INET6, SOCK_STREAM, 0);

      memset(&bink_serv_addr6, 0, sizeof(struct sockaddr_in6));

      bink_serv_addr6.sin6_family = AF_INET6;
      bink_serv_addr6.sin6_addr = in6addr_any;
      bink_serv_addr6.sin6_port = htons(binkport);

      if (setsockopt(binkfd6, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&on, sizeof(on)) < 0) {
        std::cerr << err() << ts() << "BinkpServer : Error setting IPV6_V6ONLY (Binkp - ipv6)" << rst() << std::endl;
        return -1;
      }
      if (setsockopt(binkfd6, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0) {
        std::cerr << err() << ts() << "BinkpServer : Error setting SO_REUSEADDR (Binkp - ipv6)" << rst() << std::endl;
        return -1;
      }
      if (setsockopt(binkfd6, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on)) < 0) {
        std::cerr << err() << ts() << "BinkpServer : Error setting TCP_NODELAY (Binkp - ipv6)" << rst() << std::endl;
        return -1;
      }
      if (bind(binkfd6, (struct sockaddr *)&bink_serv_addr6, sizeof(struct sockaddr_in6)) < 0) {
        std::cerr << err() << ts() << "BinkpServer : Error binding. (Binkp - ipv6)" << rst() << std::endl;
        return -1;
      }

      listen(binkfd6, 5);
      std::cout << norm() << ts() << "BinkpServer : Listening on port " << binkport << "(Binkp - ipv6)" << rst() << std::endl;
    }
    listen(binkfd, 5);
    std::cout << norm() << ts() << "BinkpServer : Listening on port " << binkport << "(Binkp)" << rst() << std::endl;
  }

  if (httpport != -1 && httproot != "") {
    std::cout << norm() << ts() << "HttpServer  : Launching port " << httpport << "(HTTP)" << rst() << std::endl;
#ifdef _MSC_VER
    std::stringstream ss;
    ss.str("");
    ss << "\"httpsrv.exe\" " << httpport << " \"" << httproot << "\"";

    if (ipv6) {
      ss << " -6";
    }

    char *cmd = strdup(ss.str().c_str());

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    //	si.dwFlags = STARTF_USESTDHANDLES;
    //	si.hStdInput = INVALID_HANDLE_VALUE;
    //	si.hStdError = INVALID_HANDLE_VALUE;
    //	si.hStdOutput = INVALID_HANDLE_VALUE;

    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
      std::cerr << err() << ts() << "HttpServer: Failed to create process!" << rst() << std::endl;
      free(cmd);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    free(cmd);
#else
    pid_t pid = fork();
    if (pid == 0) {
      snprintf(sockstr, 10, "%d", csockfd);
      if (ipv6) {
        if (execlp("./httpsrv", "./httpsrv", std::to_string(httpport).c_str(), httproot.c_str(), "-6", NULL) == -1) {
          perror("Execlp: ");
          exit(-1);
        }
      } else {
        if (execlp("./httpsrv", "./httpsrv", std::to_string(httpport).c_str(), httproot.c_str(), NULL) == -1) {
          perror("Execlp: ");
          exit(-1);
        }
      }
    } else if (pid == -1) {
      std::cerr << err() << ts() << "HttpServer: Failed to create process!" << rst() << std::endl;
    }
#endif
  }

  int nfds;
  int maxfd = telnetfd;
  fd_set server_fds;
  FD_ZERO(&server_fds);
  FD_SET(telnetfd, &server_fds);
  if (gopherport != -1) {
    FD_SET(gopherfd, &server_fds);
    if (gopherfd > maxfd) {
      maxfd = gopherfd;
    }
  }
  if (nntpport != -1) {
    FD_SET(nntpfd, &server_fds);
    if (nntpfd > maxfd) {
      maxfd = nntpfd;
    }
  }

  if (binkport != -1) {
    FD_SET(binkfd, &server_fds);
    if (binkfd > maxfd) {
      maxfd = binkfd;
    }
  }

  if (sshport != -1) {
    FD_SET(sshfd, &server_fds);

    if (sshfd > maxfd) {
      maxfd = sshfd;
    }
  }

  if (ipv6) {
    FD_SET(telnetfd6, &server_fds);
    if (telnetfd6 > maxfd)
      maxfd = telnetfd6;

    if (binkport != -1) {
      FD_SET(binkfd6, &server_fds);
      if (binkfd6 > maxfd) {
        maxfd = binkfd6;
      }
    }

    if (gopherport != -1) {
      FD_SET(gopherfd6, &server_fds);
      if (gopherfd6 > maxfd) {
        maxfd = gopherfd6;
      }
    }

    if (nntpport != -1) {
      FD_SET(nntpfd6, &server_fds);
      if (nntpfd6 > maxfd) {
        maxfd = nntpfd6;
      }
    }

    if (sshport != -1) {
      FD_SET(sshfd6, &server_fds);

      if (sshfd6 > maxfd) {
        maxfd = sshfd6;
      }
    }
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
    if (sshport != -1) {
      if (FD_ISSET(sshfd, &copy_fds)) {
        csockfd = accept(sshfd, (struct sockaddr *)&client_addr, (socklen_t *)&clen);
      }
      if (ipv6) {
        if (FD_ISSET(sshfd6, &copy_fds)) {
          csockfd = accept(sshfd6, (struct sockaddr *)&client_addr6, (socklen_t *)&clen6);
          ipv6con = true;
        }
      }
    }
    if (binkport != -1) {
      if (FD_ISSET(binkfd, &copy_fds)) {
        csockfd = accept(binkfd, (struct sockaddr *)&client_addr, (socklen_t *)&clen);
#ifdef _MSC_VER
        std::stringstream ss;
        ss.str("");
        ss << "\"binki.exe\" -S " << csockfd;

        char *cmd = strdup(ss.str().c_str());

        std::cout << norm() << ts() << "BinkpServer : Launching Binki" << rst() << std::endl;

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        //	si.dwFlags = STARTF_USESTDHANDLES;
        //	si.hStdInput = INVALID_HANDLE_VALUE;
        //	si.hStdError = INVALID_HANDLE_VALUE;
        //	si.hStdOutput = INVALID_HANDLE_VALUE;

        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
          std::cerr << err() << ts() << "BinkpServer: Failed to create process!" << rst() << std::endl;
          free(cmd);
          closesocket(csockfd);
          continue;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        free(cmd);
        closesocket(csockfd);
#else
        pid_t pid = fork();
        if (pid == 0) {
          snprintf(sockstr, 10, "%d", csockfd);
          if (execlp("./binki", "./binki", "-S", sockstr, NULL) == -1) {
            perror("Execlp: ");
            exit(-1);
          }
        } else if (pid == -1) {
          std::cerr << err() << ts() << "BinkpServer: Failed to create process!" << rst() << std::endl;
          close(csockfd);
        } else {
          close(csockfd);
        }
#endif
        continue;
      }
      if (ipv6) {
        if (FD_ISSET(binkfd6, &copy_fds)) {
          csockfd = accept(binkfd6, (struct sockaddr *)&client_addr6, (socklen_t *)&clen6);
#ifdef _MSC_VER
          std::stringstream ss;
          ss.str("");
          ss << "\"binki.exe\" -S " << csockfd;

          char *cmd = strdup(ss.str().c_str());

          std::cout << norm() << ts() << "BinkpServer : Launching Binki (IPv6)" << rst() << std::endl;

          STARTUPINFOA si;
          PROCESS_INFORMATION pi;

          ZeroMemory(&si, sizeof(si));
          si.cb = sizeof(si);
          //	si.dwFlags = STARTF_USESTDHANDLES;
          //	si.hStdInput = INVALID_HANDLE_VALUE;
          //	si.hStdError = INVALID_HANDLE_VALUE;
          //	si.hStdOutput = INVALID_HANDLE_VALUE;

          ZeroMemory(&pi, sizeof(pi));

          if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
            std::cerr << err() << ts() << "BinkpServer: Failed to create process!" << rst() << std::endl;
            free(cmd);
            closesocket(csockfd);
            continue;
          }
          CloseHandle(pi.hProcess);
          CloseHandle(pi.hThread);
          free(cmd);
          closesocket(csockfd);
#else
          pid_t pid = fork();
          if (pid == 0) {
            snprintf(sockstr, 10, "%d", csockfd);
            if (execlp("./binki", "./binki", "-S", sockstr, NULL) == -1) {
              perror("Execlp: ");
              exit(-1);
            }
          } else if (pid == -1) {
            std::cerr << err() << ts() << "BinkpServer: Failed to create process!" << rst() << std::endl;
            close(csockfd);
          } else {
            close(csockfd);
          }
#endif
          continue;
        }
      }
    }

    if (gopherport != -1) {
      if (FD_ISSET(gopherfd, &copy_fds)) {
        csockfd = accept(gopherfd, (struct sockaddr *)&client_addr, (socklen_t *)&clen);
#ifdef _MSC_VER
        std::stringstream ss;
        ss.str("");
        ss << "\"gofer.exe\" " << csockfd;

        char *cmd = strdup(ss.str().c_str());

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        //	si.dwFlags = STARTF_USESTDHANDLES;
        //	si.hStdInput = INVALID_HANDLE_VALUE;
        //	si.hStdError = INVALID_HANDLE_VALUE;
        //	si.hStdOutput = INVALID_HANDLE_VALUE;

        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
          std::cerr << err() << ts() << "GopherServer: Failed to create process!" << rst() << std::endl;
          free(cmd);
          closesocket(csockfd);
          continue;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        free(cmd);
        closesocket(csockfd);
#else
        pid_t pid = fork();
        if (pid == 0) {
          snprintf(sockstr, 10, "%d", csockfd);
          if (execlp("./gofer", "./gofer", sockstr, NULL) == -1) {
            perror("Execlp: ");
            exit(-1);
          }
        } else if (pid == -1) {
          std::cerr << err() << ts() << "GopherServer: Failed to create process!" << rst() << std::endl;
          close(csockfd);
        } else {
          close(csockfd);
        }
#endif
        continue;
      }
      if (ipv6) {
        if (FD_ISSET(gopherfd6, &copy_fds)) {
          csockfd = accept(gopherfd6, (struct sockaddr *)&client_addr6, (socklen_t *)&clen6);
#ifdef _MSC_VER
          std::stringstream ss;
          ss.str("");
          ss << "\"gofer.exe\" " << csockfd;

          char *cmd = strdup(ss.str().c_str());

          STARTUPINFOA si;
          PROCESS_INFORMATION pi;

          ZeroMemory(&si, sizeof(si));
          si.cb = sizeof(si);
          //	si.dwFlags = STARTF_USESTDHANDLES;
          //	si.hStdInput = INVALID_HANDLE_VALUE;
          //	si.hStdError = INVALID_HANDLE_VALUE;
          //	si.hStdOutput = INVALID_HANDLE_VALUE;

          ZeroMemory(&pi, sizeof(pi));

          if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            std::cerr << err() << ts() << "GopherServer: Failed to create process!" << rst() << std::endl;
            free(cmd);
            closesocket(csockfd);
            continue;
          }
          CloseHandle(pi.hProcess);
          CloseHandle(pi.hThread);
          free(cmd);
          closesocket(csockfd);
#else
          pid_t pid = fork();
          if (pid == 0) {
            snprintf(sockstr, 10, "%d", csockfd);
            if (execlp("./gofer", "./gofer", sockstr, NULL) == -1) {
              perror("Execlp: ");
              exit(-1);
            }
          } else if (pid == -1) {
            std::cerr << err() << ts() << "GopherServer: Failed to create process!" << rst() << std::endl;
            close(csockfd);
          } else {
            close(csockfd);
          }
#endif
          continue;
        }
      }
    }
    if (nntpport != -1) {
      if (FD_ISSET(nntpfd, &copy_fds)) {
        csockfd = accept(nntpfd, (struct sockaddr *)&client_addr, (socklen_t *)&clen);
#ifdef _MSC_VER
        std::stringstream ss;
        ss.str("");
        ss << "\"newssrv.exe\" " << csockfd;

        char *cmd = strdup(ss.str().c_str());

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        //	si.dwFlags = STARTF_USESTDHANDLES;
        //	si.hStdInput = INVALID_HANDLE_VALUE;
        //	si.hStdError = INVALID_HANDLE_VALUE;
        //	si.hStdOutput = INVALID_HANDLE_VALUE;

        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
          std::cerr << err() << ts() << "NNTPServer  : Failed to create process!" << rst() << std::endl;
          free(cmd);
          closesocket(csockfd);
          continue;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        free(cmd);
        closesocket(csockfd);
#else
        pid_t pid = fork();
        if (pid == 0) {
          snprintf(sockstr, 10, "%d", csockfd);
          if (execlp("./newssrv", "./newssrv", sockstr, NULL) == -1) {
            perror("Execlp: ");
            exit(-1);
          }
        } else if (pid == -1) {
          std::cerr << err() << ts() << "NNTPServer  : Failed to create process!" << rst() << std::endl;
          close(csockfd);
        } else {
          close(csockfd);
        }
#endif
        continue;
      }
      if (ipv6) {
        if (FD_ISSET(nntpfd6, &copy_fds)) {
          csockfd = accept(nntpfd6, (struct sockaddr *)&client_addr6, (socklen_t *)&clen6);
#ifdef _MSC_VER
          std::stringstream ss;
          ss.str("");
          ss << "\"newssrv.exe\" " << csockfd;

          char *cmd = strdup(ss.str().c_str());

          STARTUPINFOA si;
          PROCESS_INFORMATION pi;

          ZeroMemory(&si, sizeof(si));
          si.cb = sizeof(si);
          //	si.dwFlags = STARTF_USESTDHANDLES;
          //	si.hStdInput = INVALID_HANDLE_VALUE;
          //	si.hStdError = INVALID_HANDLE_VALUE;
          //	si.hStdOutput = INVALID_HANDLE_VALUE;

          ZeroMemory(&pi, sizeof(pi));

          if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            std::cerr << err() << ts() << "NNTPServer  : Failed to create process!" << rst() << std::endl;
            free(cmd);
            closesocket(csockfd);
            continue;
          }
          CloseHandle(pi.hProcess);
          CloseHandle(pi.hThread);
          free(cmd);
          closesocket(csockfd);
#else
          pid_t pid = fork();
          if (pid == 0) {
            snprintf(sockstr, 10, "%d", csockfd);
            if (execlp("./newssrv", "./newssrv", sockstr, NULL) == -1) {
              perror("Execlp: ");
              exit(-1);
            }
          } else if (pid == -1) {
            std::cerr << err() << ts() << "NNTPServer  : Failed to create process!" << rst() << std::endl;
            close(csockfd);
          } else {
            close(csockfd);
          }
#endif
          continue;
        }
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
            if (sshport == -1) {
              close(telnetfd);
            } else {
              close(telnetfd);
              close(sshfd);
            }

            snprintf(sockstr, 10, "%d", csockfd);
            snprintf(nodestr, 10, "%lu", i + 1);
            if (telnet) {
              std::cout << norm() << ts() << "NodeManager : Launching envy (Telnet - " << ipaddr << ")" << rst() << std::endl;
              if (execlp("./envy", "./envy", "-S", sockstr, "-N", nodestr, "-T", NULL) == -1) {
                perror("Execlp: ");
                exit(-1);
              }
            } else {
              std::cout << norm() << ts() << "NodeManager : Launching envy (SSH - " << ipaddr << ")" << rst() << std::endl;
              if (execlp("./envy", "./envy", "-S", sockstr, "-N", nodestr, "-SSH", NULL) == -1) {
                perror("Execlp: ");
                exit(-1);
              }
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

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started:
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
