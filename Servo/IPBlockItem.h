#pragma once
#include <ctime>
#include <string>
#include <vector>

class IPBlockItem {
public:
  IPBlockItem(std::string ipaddress, std::string data_path, bool block, bool pass, time_t span, int count);
  ~IPBlockItem();
  bool should_pass();
  std::string getip();

private:
  std::string data_path;
  std::string ipaddr;
  bool passlist;
  bool blocklist;
  int times;
  time_t first_try;
  time_t span;
  int count;
};
