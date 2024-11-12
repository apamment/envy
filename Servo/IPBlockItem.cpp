#include "IPBlockItem.h"
#include <ctime>
#include <string>
#include <vector>

IPBlockItem::IPBlockItem(std::string ipaddress, std::string datapath, bool block, bool pass, time_t span, int count) {
  blocklist = block;
  passlist = pass;
  ipaddr = ipaddress;
  first_try = time(NULL);
  times = 0;
  data_path = datapath;
  this->span = span;
  this->count = count;
}

IPBlockItem::~IPBlockItem() {}

bool IPBlockItem::should_pass() {
  time_t curtime = time(NULL);

  if (blocklist) {
    return false;
  }

  if (passlist) {
    return true;
  }

  if (curtime > first_try + span) {
    first_try = curtime;
    times = 0;
    return true;
  } else {
    times++;
    if (times > count) {
      blocklist = true;
      FILE *fptr = fopen(std::string(data_path + "/blocklist.ip").c_str(), "a");
      if (fptr) {
        fprintf(fptr, "%s\n", ipaddr.c_str());
        fclose(fptr);
      }
      return false;
    }
  }

  return true;
}

std::string IPBlockItem::getip() { return ipaddr; }
