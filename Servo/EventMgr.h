#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct event_t {
  std::string name;
  std::string execute;
  int interval;
  int start_day;
  int start_hour;
  int start_minute;
  time_t nextrun;
  std::string file_to_watch;
  bool file_exists;
  std::filesystem::file_time_type modified_time;
};

class EventMgr {
public:
  void run(std::string datapath);
  static void executor(EventMgr *ev);
  std::vector<struct event_t> events;
  bool shutdown;

private:
  bool load_config(std::string datapath);
};
