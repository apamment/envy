#include <cstdio>
#include <cstring>
#include "Disconnect.h"
#include "Node.h"

int main(int argc, char **argv) {
  int node = 0;
  int sock = 0;
  bool telnet = false;
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
