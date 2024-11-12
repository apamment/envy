#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#ifdef _MSC_VER
#include <Windows.h>
#define strncasecmp strnicmp
#else
#include <unistd.h>
#endif
#include "../Common/toml.hpp"
#include "EventMgr.h"

extern std::string ts();
extern std::string err();
extern std::string norm();
extern std::string rst();

bool EventMgr::load_config(std::string datapath) {
  try {
    auto data = toml::parse_file(datapath + "/events.toml");

    auto eventitems = data.get_as<toml::array>("event");

    for (size_t i = 0; i < eventitems->size(); i++) {
      std::string myname;
      int myinterval;
      std::string myexec;
      std::string myfiletowatch;
      int mystart_day;
      int mystart_hour;
      int mystart_minute;

      auto itemtable = eventitems->get(i)->as_table();

      auto name = itemtable->get("name");
      if (name != nullptr) {
        myname = name->as_string()->value_or("Unnamed Event");
      } else {
        myname = "Unnamed Event";
      }

      auto interval = itemtable->get("interval");
      if (interval != nullptr) {
        myinterval = interval->as_integer()->value_or(0);
      } else {
        myinterval = 0;
      }

      auto start_day = itemtable->get("start");
      if (start_day != nullptr) {
        std::string startstr = start_day->as_string()->value_or("");
        mystart_day = 0;
        mystart_hour = 0;
        mystart_minute = 0;

        if (startstr.find(",") != std::string::npos && startstr.find(":") != std::string::npos) {

          std::string startday = startstr.substr(0, startstr.find(","));
          std::string starttime = startstr.substr(startstr.find(",") + 1);

          if (strncasecmp(startday.c_str(), "sun", 3) == 0) {
            mystart_day = 0;
          } else if (strncasecmp(startday.c_str(), "mon", 3) == 0) {
            mystart_day = 1;
          } else if (strncasecmp(startday.c_str(), "tue", 3) == 0) {
            mystart_day = 2;
          } else if (strncasecmp(startday.c_str(), "wed", 3) == 0) {
            mystart_day = 3;
          } else if (strncasecmp(startday.c_str(), "thu", 3) == 0) {
            mystart_day = 4;
          } else if (strncasecmp(startday.c_str(), "fri", 3) == 0) {
            mystart_day = 5;
          } else if (strncasecmp(startday.c_str(), "sat", 3) == 0) {
            mystart_day = 6;
          }

          mystart_hour = stoi(starttime.substr(0, starttime.find(":")));
          mystart_minute = stoi(starttime.substr(starttime.find(":") + 1));
        }
      } else {
        mystart_day = 0;
        mystart_hour = 0;
        mystart_minute = 0;
      }

      auto filetowatch = itemtable->get("watchfile");
      if (filetowatch != nullptr) {
        myfiletowatch = filetowatch->as_string()->value_or("");
      } else {
        myfiletowatch = "";
      }

      auto exe = itemtable->get("exec");
      if (exe != nullptr) {
        myexec = exe->as_string()->value_or("");
      } else {
        myexec = "";
      }

      if ((myinterval == 0 && myfiletowatch == "") || myexec == "" || (myinterval != 0 && myfiletowatch != "")) {
        std::cerr << err() << ts() << "EventManager: Invalid event config for " << myname << rst() << std::endl;
        continue;
      }

      if (myinterval > 10080) {
        std::cerr << err() << ts() << "EventManager: Event Interval greater than 1 week for " << myname << "." << rst() << std::endl;
        continue;
      }

      struct event_t ev;

      ev.name = myname;
      ev.execute = myexec;
      ev.interval = myinterval;
      ev.start_day = mystart_day;
      ev.start_hour = mystart_hour;
      ev.start_minute = mystart_minute;

      if (myfiletowatch != "") {
        ev.file_to_watch = myfiletowatch;

        if (std::filesystem::exists(myfiletowatch)) {
          ev.file_exists = true;
          ev.modified_time = std::filesystem::last_write_time(myfiletowatch);
        } else {
          ev.file_exists = false;
          ev.modified_time = std::filesystem::file_time_type::clock::now();
        }
      }

      if (ev.interval != 0) {
        time_t now = time(NULL);
        struct tm start_tm;

#ifdef _MSC_VER
        localtime_s(&start_tm, &now);
#else
        localtime_r(&now, &start_tm);
#endif
        start_tm.tm_mday = start_tm.tm_mday % 7 + (ev.start_day - start_tm.tm_wday);
        start_tm.tm_hour = ev.start_hour;
        start_tm.tm_min = ev.start_minute;
        start_tm.tm_sec = 0;

        time_t starttime = mktime(&start_tm);

        if (starttime > now) {
          starttime -= 604800;
        }

        ev.nextrun = now + ((myinterval * 60) - (std::abs(starttime - now) % (myinterval * 60)));

        // std::cout << "Next run in " << (ev.nextrun - now) << " seconds..." << std::endl;
      }
      events.push_back(ev);
    }
  } catch (toml::parse_error const &) {
    return false;
  }

  std::cout << norm() << ts() << "EventManager: Loaded " << events.size() << " events..." << rst() << std::endl;

  return true;
}

void EventMgr::executor(EventMgr *ev) {
  while (!ev->shutdown) {
#ifdef _MSC_VER
    Sleep(60000);
#else
    sleep(60);
#endif
    time_t now = time(NULL);

    for (size_t i = 0; i < ev->events.size(); i++) {
      bool shouldrun = false;
      std::string reason = "";
      if (ev->events.at(i).interval > 0) {
        if (ev->events.at(i).nextrun <= now) {
          ev->events.at(i).nextrun = now + (ev->events.at(i).interval * 60);
          reason = "Timed";
          shouldrun = true;
        }
      } else {
        std::filesystem::path fspath(ev->events.at(i).file_to_watch);

        if (ev->events.at(i).file_exists != std::filesystem::exists(fspath)) {
          shouldrun = true;
          ev->events.at(i).file_exists = std::filesystem::exists(fspath);
          if (!ev->events.at(i).file_exists) {
            reason = "File Deleted";
          } else {
            reason = "File Created";
            ev->events.at(i).modified_time = std::filesystem::last_write_time(fspath);
          }
        } else if (std::filesystem::exists(fspath) && ev->events.at(i).modified_time != std::filesystem::last_write_time(fspath)) {
          shouldrun = true;
          ev->events.at(i).modified_time = std::filesystem::last_write_time(fspath);
          reason = "File Modified";
        }
      }

      if (shouldrun) {
        if (ev->events.at(i).name != "SILENT") {
          std::cout << norm() << ts() << "EventManager: Running " << ev->events.at(i).name << " (Reason: " << reason << ")" << rst() << std::endl;
        }

#ifdef _MSC_VER
        char *cmd = strdup(ev->events.at(i).execute.c_str());

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;

        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_MINIMIZE;

        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
          std::cerr << err() << ts() << "EventManager: Failed to create process!" << rst() << std::endl;
          free(cmd);
          continue;
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        free(cmd);
#else

        pid_t pid = fork();
        if (pid == 0) {
          std::stringstream ss;
          execl("/bin/sh", "sh", "-c", ev->events.at(i).execute.c_str(), (char *)NULL);
          exit(0);
        } else if (pid == -1) {
          std::cerr << err() << ts() << "EventManager: Failed to create process!" << rst() << std::endl;
        }
#endif
      }
    }
  }
}

void EventMgr::run(std::string datapath) {
  // load events
  if (!load_config(datapath)) {
    std::cerr << err() << ts() << "EventManager: Error loading event manager config" << rst() << std::endl;
    return;
  }

  shutdown = false;
  // run event thread
  std::thread th(executor, this);
  th.detach();
  // return
}
