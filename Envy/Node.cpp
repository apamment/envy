#include <sys/socket.h>
#include <cstring>
#include <cstdarg>
#include <cerrno>
#include <unistd.h>
#include <filesystem>
#include <sstream>
#include <fstream>
#include "../Common/INIReader.h"
#include "../Common/toml.hpp"
#include "Node.h"
#include "MessageBase.h"
#include "FileBase.h"
#include "Disconnect.h"
#include "Script.h"
#include "User.h"
#include "Door.h"
#include "Version.h"

Node::Node(int node, int socket, bool telnet) {
  this->node = node;
  this->socket = socket;
  this->telnet = telnet;
  this->ansi_supported = true;
  this->uid = 0;
  this->timeleft = 15 * 60;
  this->last_time_check = 0;
  tstage = 0;
};

void Node::action(std::string s) {
  std::ofstream of;

  std::filesystem::path fpath;
  fpath.append(tmp_path);
  fpath.append(std::to_string(node));
  if (!std::filesystem::exists(fpath)) {
    std::filesystem::create_directories(fpath);
  }

  of.open(tmp_path + "/" + std::to_string(node) + "/node.use", std::ofstream::out | std::ofstream::trunc);

  if (of.is_open()) {
    of << uid << std::endl;
    of << s << std::endl;
    of.close();
  }

  log->log(LOG_INFO, "%s -> %s", username.c_str(), s.c_str());
}

std::vector<struct nodeuse_s> Node::get_actions() {
  std::vector<struct nodeuse_s> ret;
  for (int i = 0; i < max_nodes; i++) {
    std::ifstream ifs;

    ifs.open(tmp_path + "/" + std::to_string(i + 1) + "/node.use");
    struct nodeuse_s nu;
    if (ifs.is_open()) {
      std::string s;

      getline(ifs, s);
      nu.uid = std::stoi(s);
      getline(ifs, s);
      nu.action = s;
      ret.push_back(nu);
      ifs.close();
    } else {
      nu.uid = 0;
      nu.action = "Waiting for caller...";
      ret.push_back(nu);
    }
  }
  return ret;
}

void Node::pause() {
  if (ansi_supported) {
    bprintf("\x1b[s|10Press a key..|07");
    getch();
    bprintf("\x1b[u\x1b[K");
  } else {
    bprintf("Press a key...");
    getch();
    bprintf("\r\n");
  }
}

void Node::disconnect() {
  close(socket);
  if (std::filesystem::exists(tmp_path + "/" + std::to_string(node) + "/node.use")) {
    std::filesystem::remove(tmp_path + "/" + std::to_string(node) + "/node.use");
  }
  throw(DisconnectException("Disconnected"));
}

void Node::cls() {
  if (ansi_supported) {
    bprintf("\x1b[2J\x1b[1;1H");
  } else {
    bprintf("\r\n\r\n");
  }
}

bool Node::detectANSI() {
  bprintf("\x1b[s\x1b[999;999H\x1b[6n\x1b[u");
  char buffer[1024];
  timeval t;
  time_t then = time(NULL);
  t.tv_sec = 1;
  t.tv_usec = 0;
  time_t now;
  int len;
  int gotnum = 0;
  int gotnum1 = 0;
  size_t w = 0;
  size_t h = 0;
  do {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(socket, &fds);

    if (select(socket + 1, &fds, NULL, NULL, &t) < 0) {
      return false;
    }

    if (FD_ISSET(socket, &fds)) {
      len = recv(socket, buffer, 1024, 0);
      if (len == 0) {
        disconnect();
      }
      for (int i = 0; i < len; i++) {
        if (buffer[i] == '\x1b' && buffer[i + 1] == '[') {
          for (int j = i + 2; j < len; j++) {
            switch (buffer[j]) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
              if (gotnum1) {
                w = w * 10 + (buffer[j] - '0');
              } else {
                h = h * 10 + (buffer[j] - '0');
              }
              gotnum = 1;
              break;
            case ';':
              gotnum1 = 1;
              gotnum = 0;
              break;
            case 'R':
              if (gotnum && gotnum1) {
                if (w != term_width || h != term_height) {
                  term_width = w;
                  term_height = h;
                }
                return true;
              }
              break;
            }
          }
        }
      }
    }

    now = time(NULL);
  } while (now - then < 5);

  return false;
}

void Node::bprintf_nc(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  bprintf(false, fmt, args);
  va_end(args);
}

void Node::bprintf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  bprintf(true, fmt, args);
  va_end(args);
}

void Node::bprintf(bool parse_pipes, const char *fmt, va_list args) {
  char buffer[2048];

  vsnprintf(buffer, sizeof buffer, fmt, args);

  std::stringstream ss;

  for (size_t i = 0; i < strlen(buffer); i++) {
    if (buffer[i] == '|' && parse_pipes == true) {
      if (i <= strlen(buffer) - 3) {
        int pipe_color = 0;
        if (buffer[i + 1] >= '0' && buffer[i + 1] <= '9' && buffer[i + 2] >= '0' && buffer[i + 2] <= '9') {
          pipe_color = (buffer[i + 1] - '0') * 10 + (buffer[i + 2] - '0');

          if (ansi_supported) {
            ss << "\x1b[";
            switch (pipe_color) {
            case 0:
              ss << "0;30";
              break;
            case 1:
              ss << "0;34";
              break;
            case 2:
              ss << "0;32";
              break;
            case 3:
              ss << "0;36";
              break;
            case 4:
              ss << "0;31";
              break;
            case 5:
              ss << "0;35";
              break;
            case 6:
              ss << "0;33";
              break;
            case 7:
              ss << "0;37";
              break;
            case 8:
              ss << "1;30";
              break;
            case 9:
              ss << "1;34";
              break;
            case 10:
              ss << "1;32";
              break;
            case 11:
              ss << "1;36";
              break;
            case 12:
              ss << "1;31";
              break;
            case 13:
              ss << "1;35";
              break;
            case 14:
              ss << "1;33";
              break;
            case 15:
              ss << "1;37";
              break;
            case 16:
              ss << "40";
              break;
            case 17:
              ss << "44";
              break;
            case 18:
              ss << "42";
              break;
            case 19:
              ss << "46";
              break;
            case 20:
              ss << "41";
              break;
            case 21:
              ss << "45";
              break;
            case 22:
              ss << "43";
              break;
            case 23:
              ss << "47";
              break;
            }
            ss << "m";
          }
          i += 2;
          continue;
        }
      }
    }
    ss << buffer[i];
  }

  send(socket, ss.str().c_str(), ss.str().length(), 0);
}

void Node::putfile(std::string filename) {
  std::ifstream t(filename);
  char ch;

  if (t.is_open()) {
    while (!t.eof()) {
      t.get(ch);
      if (ch == 0x1a)
        break;
      if (ch != '\n') {
        send(socket, &ch, 1, 0);
      }
      if (ch == '\r') {
        send(socket, "\r\n", 2, 0);
      }
    }
    /*
        if (ch != '\r' && ch != '\n') {
          send(socket, "\r\n", 2, 0);
        }
    */
    t.close();
  }
}

bool Node::putgfile(std::string gfile) {
  std::filesystem::path ascii(gfile_path + "/" + gfile + ".asc");
  std::filesystem::path ansi(gfile_path + "/" + gfile + ".ans");

  if (std::filesystem::exists(ansi) && ansi_supported) {
    putfile(ansi.u8string());
    return true;
  } else if (std::filesystem::exists(ascii)) {
    putfile(ascii.u8string());
    return true;
  }

  return false;
}

char Node::getch() { return getch_real(false); }

char Node::getch_real(bool shouldtimeout) {
  unsigned char c;
  unsigned char dowillwontdont = 0;
  struct timeval tv;
  int timeout = 0;

  if (!time_check()) {
    bprintf("\r\n\r\n|14Sorry, you're out of time for today!|07\r\n");
    disconnect();
  }

  while (true) {
    fd_set rfd;
    FD_ZERO(&rfd);
    FD_SET(socket, &rfd);

    tv.tv_sec = 60;
    tv.tv_usec = 0;

    int rs = select(socket + 1, &rfd, NULL, NULL, &tv);

    if (rs == 0) {
      timeout++;

      if (timeout > (uid > 0 ? get_timeout() : 15)) {
        bprintf("\r\n\r\n|14You've timed out, call back when you're there!|07\r\n");
        disconnect();
      }

      if (!time_check()) {
        bprintf("\r\n\r\n|14Sorry, you're out of time for today!|07\r\n");
        disconnect();
      }
      if (shouldtimeout) {
        return -1;
      }
    } else if (rs == -1 && errno != EINTR) {
      disconnect();
    } else if (FD_ISSET(socket, &rfd)) {
      ssize_t ret = recv(socket, &c, 1, 0);
      if (ret == -1 && errno != EINTR) {
        disconnect();
      } else if (ret == 0) {
        disconnect();
      } else {
        if (tstage == 0) {
          if (c == 255) {
            tstage = 1;
            continue;
          } else if (c != '\n' && c != '\0') {
            return (char)c;
          }
        }
        if (tstage == 1) {
          if (c == 255) {
            tstage = 0;
            return (char)c;
          } else if (c == 250) {
            tstage = 3;
            continue;
          } else {
            dowillwontdont = c;
            tstage = 2;
            continue;
          }
        }
        if (tstage == 2) {

          tstage = 0;
          continue;
        }
        if (tstage == 3) {
          if (c == 240) {
            tstage = 0;
            continue;
          }
        }
      }
    }
  }
}

std::string Node::get_str(int length) { return get_str(length, 0, ""); }

std::string Node::get_str(int length, char mask) { return get_str(length, mask, ""); }

std::string Node::get_str(int length, char mask, std::string placeholder) {
  std::stringstream ss;

  placeholder = placeholder.substr(0, length - 1);

  if (ansi_supported) {
    bprintf("\033[1;37;42m");

    if (placeholder.length() > 0) {
      for (int i = 0; i < length; i++) {
        if (i < placeholder.length() && i < length - 1) {
          if (mask) {
            bprintf("%c", mask);
          } else {
            bprintf("%c", placeholder.at(i));
          }
          ss << placeholder.at(i);
        }
      }
    }

    bprintf("\033[s");
    for (int i = placeholder.length(); i < length; i++) {
      bprintf(" ");
    }

    bprintf("\033[u\033[1;37;42m");
  } else {
    bprintf("%s", placeholder.c_str());
    ss << placeholder;
  }

  while (ss.str().length() < length) {
    char c = getch();
    if (c == '\b' || c == 127) {
      if (ss.str().length() > 0) {
        std::string s = ss.str();
        s.erase(s.length() - 1, 1);
        if (ansi_supported) {
          bprintf("\033[D \033[D");
        } else {
          bprintf("\b \b");
        }
        ss.str("");
        ss << s;
      }
    } else if (c == '\r') {
      if (ansi_supported) {
        bprintf("\033[0m\r\n");
      }
      return ss.str();
    } else if (c != '\n') {
      if (mask != 0) {
        bprintf("%c", mask);
      } else {
        bprintf("%c", c);
      }
      ss << c;
    }
  }
  if (ansi_supported) {
    bprintf("\033[0m\r\n");
  }
  return ss.str();
}

int Node::run() {

  const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  std::string cmdscript;
  unsigned char iac_echo[] = {IAC, IAC_WILL, IAC_ECHO, '\0'};
  unsigned char iac_sga[] = {IAC, IAC_WILL, IAC_SUPPRESS_GO_AHEAD, '\0'};

  INIReader inir("envy.ini");
  int newuserseclevel = 10;
  if (inir.ParseError() != 0) {
    return -1;
  }

  // read config
  gfile_path = inir.Get("paths", "gfile path", "gfiles");
  data_path = inir.Get("paths", "data path", "data");
  script_path = inir.Get("paths", "scripts path", "scripts");
  log_path = inir.Get("paths", "log path", "logs");
  msg_path = inir.Get("paths", "message path", "msgs");
  tmp_path = inir.Get("paths", "temp path", "tmp");
  default_tagline = inir.Get("main", "default tagline", "Unknown BBS");
  bbsname = inir.Get("main", "bbs name", "Unknown BBS");
  opname = inir.Get("main", "sysop name", "Unknown");
  max_nodes = inir.GetInteger("main", "max nodes", 4);
  cmdscript = inir.Get("main", "command script", "menu");
  newuserseclevel = inir.GetInteger("main", "new user seclevel", 10);
  log = new Logger();
  log->load(log_path + "/envy." + std::to_string(node) + ".log");

  load_msgbases();
  load_doors();
  load_seclevels();
  load_protocols();
  load_filebases();

  log->log(LOG_INFO, "Connected!");

  // send initial telnet negotiation
  send(socket, (char *)iac_echo, 3, 0);
  send(socket, (char *)iac_sga, 3, 0);

  bprintf("Envy/%s-%s - Copyright (C) 2024; Andrew Pamment\r\n", VERSION, GITV);

  ansi_supported = detectANSI();

  putgfile("welcome");

  int tries = 0;

  while (true) {
    bool should_loop = false;

    if (tries == 5) {
      disconnect();
    }

    bprintf("USER: ");
    username = get_str(16);
    std::string password;
    std::string firstname;
    std::string lastname;
    std::string location;
    std::string email;
    if (strcasecmp(username.c_str(), "NEW") == 0) {
      log->log(LOG_INFO, "New user signing up!");
      action("New user signing up");
      cls();
      putgfile("newuser");
      while (true) {
        bprintf("Enter your desired username: ");
        username = get_str(16);
        if (username.length() == 0) {
          tries++;
          should_loop = true;
          break;
        }
        User::InvalidUserReason reason = User::valid_username(this, username);
        if (reason != User::InvalidUserReason::OK) {
          switch (reason) {
          case User::InvalidUserReason::ERROR:
            bprintf("\r\nSorry, an error occured.\r\n");
            break;
          case User::InvalidUserReason::TOOSHORT:
            bprintf("\r\nSorry, that username is too short.\r\n");
            break;
          case User::InvalidUserReason::INUSE:
            bprintf("\r\nSorry, that username is in use.\r\n");
            break;
          case User::InvalidUserReason::BADCHARS:
            bprintf("\r\nSorry, that username contains unsupported characters.\r\n");
            break;
          case User::InvalidUserReason::TRASHCAN:
            bprintf("\r\nSorry, that username is not allowed.\r\n");
            break;
          default:
            break;
          }
          continue;
        }
        break;
      }
      if (should_loop) {
        continue;
      }
      while (true) {
        bprintf("\r\nEnter your desired password: ");
        password = get_str(16, '*');
        if (password.length() == 0) {
          tries++;
          should_loop = true;
          break;
        }

        if (password.length() < 8) {
          bprintf("\r\nPassword should be at least 8 characters!\r\n");
          continue;
        }

        bprintf("\r\nRepeat your desired password: ");
        std::string password_rep = get_str(16, '*');

        if (password != password_rep) {
          bprintf("\r\nPasswords do not match!\r\n");
          continue;
        }

        break;
      }

      if (should_loop) {
        continue;
      }

      while (true) {
        bprintf("\r\nEnter your real first name: ");
        firstname = get_str(32);

        if (firstname.length() == 0) {
          tries++;
          should_loop = true;
          break;
        }

        if (firstname.find(' ') != std::string::npos) {
          bprintf("\r\nFirst name may not contain a space!\r\n");
          continue;
        }

        bprintf("\r\nEnter your real last name: ");
        lastname = get_str(32);

        if (lastname.length() == 0) {
          tries++;
          should_loop = true;
          break;
        }

        if (!User::valid_fullname(this, firstname + " " + lastname)) {
          bprintf("\r\nSorry, that real name is in use!\r\n");
          continue;
        }

        break;
      }

      if (should_loop) {
        continue;
      }

      bprintf("\r\nEnter approximate location: ");
      location = get_str(32);

      if (location.length() == 0) {
        tries++;
        continue;
      }

      while (true) {
        bprintf("\r\nEnter a contact email: ");
        email = get_str(32);

        if (email.length() == 0) {
          tries++;
          should_loop = true;
          break;
        }

        if (email.find('@') == std::string::npos) {
          bprintf("\r\nInvalid email!\r\n");
          continue;
        }

        if (email.find('.') == std::string::npos) {
          bprintf("\r\nInvalid email!\r\n");
          continue;
        }

        break;
      }

      if (should_loop) {
        continue;
      }

      uid = User::inst_user(this, username, password);

      if (uid == -1) {
        disconnect();
      } else {
        User::set_attrib(this, "fullname", firstname + " " + lastname);
        User::set_attrib(this, "location", location);
        User::set_attrib(this, "email", email);
        User::set_attrib(this, "seclevel", std::to_string(newuserseclevel));
        break;
      }
    } else {
      bprintf("PASS: ");
      password = get_str(16, '*');

      uid = User::check_password(this, username, password);

      if (uid == -1) {
        tries++;
        bprintf("\r\nInvalid Login!\r\n");
        continue;
      } else {
        break;
      }
    }
  }

  log->log(LOG_INFO, "%s logged in!", username.c_str());
  action("Logging in");
  int seclevel = get_seclevel();

  for (size_t i = 0; i < msgbases.size(); i++) {
    if (msgbases.at(i)->read_sec_level <= seclevel) {
      accessablemb.push_back(msgbases.at(i));
    }
  }

  for (size_t i = 0; i < filebases.size(); i++) {
    if (filebases.at(i)->down_sec_level <= seclevel) {
      accessablefb.push_back(filebases.at(i));
    }
  }

  curr_filebase = -1;
  std::string cfb = User::get_attrib(this, "curr_fbase", "");
  if (cfb != "") {
    for (size_t i = 0; i < accessablefb.size(); i++) {
      if (accessablefb.at(i)->database == cfb) {
        curr_filebase = i;
        break;
      }
    }
  }
  if (curr_filebase == -1) {
    if (accessablefb.size() > 0) {
      curr_filebase = 0;
    }
  }

  curr_msgbase = -1;
  std::string cmb = User::get_attrib(this, "curr_mbase", "");

  if (cmb != "") {
    for (size_t i = 0; i < accessablemb.size(); i++) {
      if (accessablemb.at(i)->file == cmb) {
        curr_msgbase = i;
        break;
      }
    }
  }
  if (curr_msgbase == -1) {
    if (accessablemb.size() > 0) {
      curr_msgbase = 0;
    }
  }

  time_t last_call = std::stoi(User::get_attrib(this, "last-call", "0"));
  int total_calls = std::stoi(User::get_attrib(this, "total-calls", "0"));

  struct tm timetm;
  struct tm nowtm;

  time_t now = time(NULL);

  localtime_r(&last_call, &timetm);
  localtime_r(&now, &nowtm);
  if (timetm.tm_yday != nowtm.tm_yday || timetm.tm_year != nowtm.tm_year) {
    timeleft = get_timeperday() * 60;
    User::set_attrib(this, "time-left", std::to_string(timeleft / 60));
  } else {
    timeleft = std::stoi(User::get_attrib(this, "time-left", std::to_string(get_timeperday()))) * 60;
  }

  total_calls++;

  User::set_attrib(this, "total-calls", std::to_string(total_calls));
  User::set_attrib(this, "last-call", std::to_string(time(NULL)));

  cls();

  if (total_calls == 1) {
    bprintf("Welcome |15%s|07, this is your first call!\r\n", username.c_str());
  } else {
    if (total_calls % 10 == 1 && total_calls % 100 != 11) {
      bprintf("Welcome back |15%s|07, this is your |15%dst|07 call!\r\n", username.c_str(), total_calls);
    } else if (total_calls % 10 == 2 && total_calls % 100 != 12) {
      bprintf("Welcome back |15%s|07, this is your |15%dnd|07 call!\r\n", username.c_str(), total_calls);
    } else if (total_calls % 10 == 3 && total_calls % 100 != 13) {
      bprintf("Welcome back |15%s|07, this is your |15%drd|07 call!\r\n", username.c_str(), total_calls);
    } else {
      bprintf("Welcome back |15%s|07, this is your |15%dth|07 call!\r\n", username.c_str(), total_calls);
    }

    if (timetm.tm_hour > 11) {
      bprintf("You last called on |15%s %d |07at |15%d:%02dpm|07\r\n", months[timetm.tm_mon], timetm.tm_mday, (timetm.tm_hour == 12 ? 12 : timetm.tm_hour - 12),
              timetm.tm_min);
    } else {
      bprintf("You last called on |15%s %d |07at |15%d:%02dam|07\r\n", months[timetm.tm_mon], timetm.tm_mday, (timetm.tm_hour == 0 ? 12 : timetm.tm_hour),
              timetm.tm_min);
    }
  }

  pause();

  Script::run(this, cmdscript);

  disconnect();

  return 0;
}

void Node::load_seclevels() {
  try {
    auto data = toml::parse_file(data_path + "/seclevels.toml");
    auto baseitems = data.get_as<toml::array>("seclevel");
    for (size_t i = 0; i < baseitems->size(); i++) {
      auto itemtable = baseitems->get(i)->as_table();
      std::string myname;
      int mylevel;
      int mytime;
      int mytimeout;
      bool myvis;

      auto level = itemtable->get("level");
      if (level != nullptr) {
        mylevel = level->as_integer()->value_or(0);
      } else {
        mylevel = 0;
      }

      auto timeout = itemtable->get("timeout");
      if (timeout != nullptr) {
        mytimeout = timeout->as_integer()->value_or(15);
      } else {
        mytimeout = 15;
      }

      auto timepday = itemtable->get("time");
      if (timepday != nullptr) {
        mytime = timepday->as_integer()->value_or(60);
      } else {
        mytime = 60;
      }

      auto vis = itemtable->get("visible");
      if (vis != nullptr) {
        myvis = vis->as_boolean()->value_or(true);
      } else {
        myvis = true;
      }

      auto name = itemtable->get("name");
      if (name != nullptr) {
        myname = name->as_string()->value_or("Level " + std::to_string(mylevel));
      } else {
        myname = "Level " + std::to_string(mylevel);
      }

      if (mylevel != 0) {
        struct seclevel_s seccfg;

        seccfg.name = myname;
        seccfg.level = mylevel;
        seccfg.time_per_day = mytime;
        seccfg.timeout = mytimeout;
        seccfg.visible = myvis;
        seclevels.push_back(seccfg);
      }
    }
  } catch (toml::parse_error const &p) {
    log->log(LOG_ERROR, "Error parsing %s/seclevels.toml, Line %d, Column %d", data_path.c_str(), p.source().begin.line, p.source().begin.column);
    log->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
  }
}

void Node::load_protocols() {
  try {
    auto data = toml::parse_file(data_path + "/protocols.toml");
    auto baseitems = data.get_as<toml::array>("protocol");

    for (size_t i = 0; i < baseitems->size(); i++) {
      auto itemtable = baseitems->get(i)->as_table();
      std::string myname;
      std::string myupcmd;
      std::string mydowncmd;
      bool myprompt;

      auto name = itemtable->get("name");
      if (name != nullptr) {
        myname = name->as_string()->value_or("Protocol " + std::to_string(i + 1));
      } else {
        myname = "Protocol " + std::to_string(i + 1);
      }
      auto upcmd = itemtable->get("upload");
      if (upcmd != nullptr) {
        myupcmd = upcmd->as_string()->value_or("");
      } else {
        myupcmd = "";
      }

      auto downcmd = itemtable->get("download");
      if (downcmd != nullptr) {
        mydowncmd = downcmd->as_string()->value_or("");
      } else {
        mydowncmd = "";
      }

      auto prompt = itemtable->get("prompt");
      if (prompt != nullptr) {
        myprompt = prompt->as_boolean()->value_or(false);
      } else {
        myprompt = false;
      }

      if (myupcmd != "" && mydowncmd != "") {
        struct protocol_s p;
        p.name = myname;
        p.upcmd = myupcmd;
        p.downcmd = mydowncmd;
        p.prompt = myprompt;
        protocols.push_back(p);
      }
    }
  } catch (toml::parse_error const &p) {
    log->log(LOG_ERROR, "Error parsing %s/protocols.toml, Line %d, Column %d", data_path.c_str(), p.source().begin.line, p.source().begin.column);
    log->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
  }
}

void Node::load_doors() {
  try {
    auto data = toml::parse_file(data_path + "/doors.toml");
    auto baseitems = data.get_as<toml::array>("door");

    for (size_t i = 0; i < baseitems->size(); i++) {
      auto itemtable = baseitems->get(i)->as_table();
      std::string mykey;
      std::string myname;
      std::string myscript;

      auto key = itemtable->get("key");
      if (key != nullptr) {
        mykey = key->as_string()->value_or("");
      } else {
        mykey = "";
      }
      auto name = itemtable->get("name");
      if (name != nullptr) {
        myname = name->as_string()->value_or(mykey);
      } else {
        myname = mykey;
      }
      auto script = itemtable->get("script");
      if (script != nullptr) {
        myscript = script->as_string()->value_or("");
      } else {
        myscript = "";
      }

      if (mykey != "" && myscript != "") {
        struct door_cfg_s doorcfg;

        doorcfg.key = mykey;
        doorcfg.name = myname;
        doorcfg.script = myscript;

        doors.push_back(doorcfg);
      }
    }
  } catch (toml::parse_error const &p) {
    log->log(LOG_ERROR, "Error parsing %s/doors.toml, Line %d, Column %d", data_path.c_str(), p.source().begin.line, p.source().begin.column);
    log->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
  }
}

void Node::launch_door(std::string key) {
  for (size_t i = 0; i < doors.size(); i++) {
    if (key == doors.at(i).key) {
      std::vector<std::string> args;
      args.push_back(std::to_string(node));

      Door::createDropfiles(this);
      Door::runExternal(this, doors.at(i).script, args, false);
      return;
    }
  }
}

void Node::load_filebases() {
  try {
    auto data = toml::parse_file(data_path + "/filebases.toml");
    auto baseitems = data.get_as<toml::array>("filebase");

    for (size_t i = 0; i < baseitems->size(); i++) {
      auto itemtable = baseitems->get(i)->as_table();
      std::string myname;
      std::string mydatabase;
      std::string myuppath;
      int mydownloadsec;
      int myuploadsec;

      auto name = itemtable->get("name");
      if (name != nullptr) {
        myname = name->as_string()->value_or("Invalid Name");
      } else {
        myname = "Unknown Name";
      }

      auto database = itemtable->get("database");
      if (database != nullptr) {
        mydatabase = database->as_string()->value_or("");
      } else {
        mydatabase = "";
      }
      auto uppath = itemtable->get("upload-path");
      if (uppath != nullptr) {
        myuppath = uppath->as_string()->value_or("");
      } else {
        myuppath = "";
      }

      auto downloads = itemtable->get("download-sec");
      if (downloads != nullptr) {
        mydownloadsec = downloads->as_integer()->value_or(10);
      } else {
        mydownloadsec = 10;
      }

      auto uploads = itemtable->get("upload-sec");
      if (uploads != nullptr) {
        myuploadsec = uploads->as_integer()->value_or(10);
      } else {
        myuploadsec = 10;
      }

      if (mydatabase != "" && myuppath != "") {
        filebases.push_back(new FileBase(myname, myuppath, mydatabase, mydownloadsec, myuploadsec));
      }
    }
  } catch (toml::parse_error const &p) {
    log->log(LOG_ERROR, "Error parsing %s/filebases.toml, Line %d, Column %d", data_path.c_str(), p.source().begin.line, p.source().begin.column);
    log->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
  }
}

void Node::load_msgbases() {
  try {
    auto data = toml::parse_file(data_path + "/msgbases.toml");
    auto baseitems = data.get_as<toml::array>("msgbase");

    for (size_t i = 0; i < baseitems->size(); i++) {
      auto itemtable = baseitems->get(i)->as_table();

      std::string myname;
      std::string myfile;
      std::string myoaddr;
      std::string mytagline;
      int myreads;
      int mywrites;
      MessageBase::MsgBaseType mytype;

      auto name = itemtable->get("name");
      if (name != nullptr) {
        myname = name->as_string()->value_or("Invalid Name");
      } else {
        myname = "Unknown Name";
      }

      auto file = itemtable->get("file");
      if (file != nullptr) {
        myfile = file->as_string()->value_or("");
      } else {
        myfile = "";
      }

      auto oaddr = itemtable->get("aka");
      if (oaddr != nullptr) {
        myoaddr = oaddr->as_string()->value_or("");
      } else {
        myoaddr = "";
      }

      auto tagline = itemtable->get("tagline");
      if (tagline != nullptr) {
        mytagline = tagline->as_string()->value_or(default_tagline);
      } else {
        mytagline = default_tagline;
      }

      auto reads = itemtable->get("read-sec");
      if (reads != nullptr) {
        myreads = reads->as_integer()->value_or(10);
      } else {
        myreads = 10;
      }

      auto writes = itemtable->get("write-sec");
      if (writes != nullptr) {
        mywrites = writes->as_integer()->value_or(10);
      } else {
        mywrites = 10;
      }

      auto mbtype = itemtable->get("type");
      if (mbtype != nullptr) {
        std::string mbtypes = mbtype->as_string()->value_or("local");

        if (strcasecmp(mbtypes.c_str(), "echomail") == 0) {
          mytype = MessageBase::MsgBaseType::ECHO;
        } else if (strcasecmp(mbtypes.c_str(), "netmail") == 0) {
          mytype = MessageBase::MsgBaseType::NETMAIL;
        } else {
          mytype = MessageBase::MsgBaseType::LOCAL;
        }
      } else {
        mytype = MessageBase::MsgBaseType::LOCAL;
      }

      if (myfile != "") {
        msgbases.push_back(new MessageBase(myname, myfile, myoaddr, mytagline, mytype, myreads, mywrites));
      }
    }

  } catch (toml::parse_error const &p) {
    log->log(LOG_ERROR, "Error parsing %s/msgbases.toml, Line %d, Column %d", data_path.c_str(), p.source().begin.line, p.source().begin.column);
    log->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
  }
}

MessageBase *Node::get_curr_msgbase() {
  if (curr_msgbase != -1) {
    return accessablemb.at(curr_msgbase);
  }

  return nullptr;
}

void Node::select_msg_base() {
  cls();

  int lines = 0;

  for (size_t i = 0; i < accessablemb.size(); i++) {
    uint32_t ur = accessablemb.at(i)->get_unread(this);
    uint32_t tot = accessablemb.at(i)->get_total(this);
    if (ur > 0) {
      bprintf("|07%4d. |15%-44.44s |10%6d |02UNREAD |13%6d |05TOTAL|07\r\n", i + 1, accessablemb.at(i)->name.c_str(), ur, tot);
    } else {
      bprintf("|08%4d. |07%-44.44s |02%6d |02UNREAD |05%6d |05TOTAL|07\r\n", i + 1, accessablemb.at(i)->name.c_str(), ur, tot);
    }
    lines++;
    if (lines == term_height - 1 || i == accessablemb.size() - 1) {
      if (i == accessablemb.size() - 1) {
        bprintf("|11END  |15- |10Select area: |07");
      } else {
        bprintf("|13MORE |15- |10Select area: |07");
      }

      std::string num = get_str(4);

      if (num.length() > 0) {
        try {
          int n = std::stoi(num);

          if (n - 1 >= 0 && n - 1 < accessablemb.size()) {
            curr_msgbase = n - 1;
            User::set_attrib(this, "curr_mbase", accessablemb.at(curr_msgbase)->file);
            return;
          }
        } catch (std::out_of_range const &) {
        } catch (std::invalid_argument const &) {
        }
      }
      lines = 0;
    }
  }
}

void Node::scan_msg_bases() {
  int lines = 0;

  for (size_t i = 0; i < accessablemb.size(); i++) {
    uint32_t ur = accessablemb.at(i)->get_unread(this);
    uint32_t tot = accessablemb.at(i)->get_total(this);
    if (ur > 0) {
      bprintf("|07%4d. |15%-44.44s |10%6d |02UNREAD |13%6d |05TOTAL|07\r\n", i + 1, accessablemb.at(i)->name.c_str(), ur, tot);
    } else {
      bprintf("|08%4d. |07%-44.44s |02%6d |02UNREAD |05%6d |05TOTAL|07\r\n", i + 1, accessablemb.at(i)->name.c_str(), ur, tot);
    }
    lines++;
    if (lines == 22) {
      pause();
      lines = 0;
    }
  }
  pause();
}

int Node::get_seclevel() { return std::stoi(User::get_attrib(this, "seclevel", "10")); }

bool Node::get_visible() {
  int seclevel = get_seclevel();

  for (size_t i = 0; i < seclevels.size(); i++) {
    if (seclevels.at(i).level == seclevel) {
      return seclevels.at(i).visible;
    }
  }
  return true;
}

int Node::get_timeperday() {
  int seclevel = get_seclevel();

  for (size_t i = 0; i < seclevels.size(); i++) {
    if (seclevels.at(i).level == seclevel) {
      return seclevels.at(i).time_per_day;
    }
  }
  return 25;
}

int Node::get_timeout() {
  int seclevel = get_seclevel();

  for (size_t i = 0; i < seclevels.size(); i++) {
    if (seclevels.at(i).level == seclevel) {
      return seclevels.at(i).timeout;
    }
  }
  return 5;
}

bool Node::time_check() {
  time_t now = time(NULL);

  if (last_time_check == 0) {
    last_time_check = now;
  }

  if (now - last_time_check >= 60) {
    timeleft -= (now - last_time_check);
    if (uid != 0) {
      User::set_attrib(this, "time-left", std::to_string(timeleft / 60));
    }
    last_time_check = now;
  }

  if (timeleft <= 0) {
    return false;
  }

  return true;
}

MessageBase *Node::get_msgbase(std::string file) {
  for (size_t i = 0; i < msgbases.size(); i++) {
    if (msgbases.at(i)->file == file) {
      return msgbases.at(i);
    }
  }
  return nullptr;
}

static std::vector<std::string> split(const std::string &s, char delim) {
  std::vector<std::string> result;
  std::stringstream ss(s);
  std::string item;

  while (getline(ss, item, delim)) {
    result.push_back(item);
  }

  return result;
}

struct protocol_s *Node::select_protocol() {
  bprintf("|10Select Protocol:|07\r\n\r\n");
  for (size_t i = 0; i < protocols.size(); i++) {
    bprintf("|10(|15%d|10) |15%s|07\r\n", i + 1, protocols.at(i).name.c_str());
  }

  bprintf("|08:> |07");
  std::string s = get_str(3);

  try {
    int num = std::stoi(s) - 1;

    if (num < 0 || num >= protocols.size()) {
      bprintf("|12Invalid Selection!\r\n");
      return nullptr;
    } else {
      return &protocols.at(num);
    }
  } catch (std::out_of_range const &) {
    bprintf("|12Invalid Selection!\r\n");
    return nullptr;
  } catch (std::invalid_argument const &) {
    bprintf("|12Invalid Selection!\r\n");
    return nullptr;
  }
}

void Node::send_file(std::string filename) {
  struct protocol_s *p = select_protocol();
  if (p == nullptr) {
    return;
  } else {
    send_file(p, filename);
  }
}

void Node::send_file(struct protocol_s *p, std::string filename) {
  std::vector<std::string> parts = split(p->downcmd, ' ');
  std::vector<std::string> args;

  for (size_t i = 1; i < parts.size(); i++) {
    if (parts.at(i) == "*f") {
      args.push_back(filename);
    } else {
      args.push_back(parts.at(i));
    }
  }

  Door::runExternal(this, parts.at(0), args, true);
}

bool Node::tag_file(struct file_s file) {
  for (size_t i = 0; i < tagged_files.size(); i++) {
    if (tagged_files.at(i).filename == file.filename) {
      return false;
    }
  }
  tagged_files.push_back(file);
  return true;
}

FileBase *Node::get_curr_filebase() {
  if (curr_filebase != -1) {
    return accessablefb.at(curr_filebase);
  }

  return nullptr;
}

void Node::clear_tagged_files() {
  bprintf("\r\n\r\n|10Cleared |15%d |10files from your tagged list.\r\n", tagged_files.size());
  tagged_files.clear();
  pause();
}

void Node::download_tagged_files() {
  if (tagged_files.size() == 0) {
    bprintf("|12Tag some files first!|07\r\n");
    pause();
    return;
  }
  struct protocol_s *p = select_protocol();

  if (p == nullptr) {
    pause();
    return;
  }

  for (size_t i = 0; i < tagged_files.size(); i++) {
    if (tagged_files.at(i).size > 0) {
      bprintf("|10Sending \"|15%s|10\"\r\nPress (q) to abort or any other key to continue...\r\n", tagged_files.at(i).filename.filename().u8string().c_str());
      char c = getch();
      if (tolower(c) == 'q') {
        tagged_files.erase(tagged_files.begin(), tagged_files.begin() + (i - 1));
        return;
      }
      send_file(p, tagged_files.at(i).filename.u8string());
      tagged_files.at(i).fb->inc_download(this, tagged_files.at(i).filename.u8string());
    }
  }
  tagged_files.clear();
}

void Node::list_tagged_files() {
  cls();

  int lines = 0;

  for (size_t i = 0; i < tagged_files.size(); i++) {
    bprintf("|07%3d. |15%s|07\r\n", i + 1, tagged_files.at(i).filename.filename().u8string().c_str());
    lines++;
    if (lines == term_height - 1) {
      pause();
      lines = 0;
    }
  }

  pause();
}

void Node::select_file_base() {
  cls();

  int lines = 0;

  for (size_t i = 0; i < accessablefb.size(); i++) {
    bprintf("|07%4d. |15%-44.44s|07\r\n", i + 1, accessablefb.at(i)->name.c_str());
    lines++;
    if (lines == term_height - 1 || i == accessablefb.size() - 1) {
      if (i == accessablefb.size() - 1) {
        bprintf("|11END  |15- |10Select area: |07");
      } else {
        bprintf("|13MORE |15- |10Select area: |07");
      }

      std::string num = get_str(4);

      if (num.length() > 0) {
        try {
          int n = std::stoi(num);

          if (n - 1 >= 0 && n - 1 < accessablefb.size()) {
            curr_filebase = n - 1;
            User::set_attrib(this, "curr_fbase", accessablefb.at(curr_filebase)->database);
            return;
          }
        } catch (std::out_of_range const &) {
        } catch (std::invalid_argument const &) {
        }
      }
      lines = 0;
    }
  }
}

void Node::upload() {
  if (get_curr_filebase()->up_sec_level > get_seclevel()) {
    bprintf("|12Sorry, you don't have access to upload to this area!|07\r\n");
    pause();
    return;
  }
  struct protocol_s *p = select_protocol();
  char *oldwd;
  std::string filename = "";

  if (p == nullptr) {
    return;
  }

  if (p->prompt) {
    bprintf("|13Please enter the filename: |07");
    filename = get_str(32);

    if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos || filename.find('*') != std::string::npos) {
      bprintf("|12Invalid filename!|07\r\n");
      return;
    }
  }
  std::string ulpath = tmp_path + "/" + std::to_string(node) + "/upload";

  if (std::filesystem::exists(ulpath)) {
    std::filesystem::remove_all(ulpath);
  }

  std::filesystem::create_directories(ulpath);

  std::vector<std::string> parts = split(p->upcmd, ' ');
  std::vector<std::string> args;

  for (size_t i = 1; i < parts.size(); i++) {
    if (parts.at(i) == "*f") {
      args.push_back(filename);
    } else {
      args.push_back(parts.at(i));
    }
  }

  oldwd = getcwd(NULL, -1);
  chdir(ulpath.c_str());

  Door::runExternal(this, parts.at(0), args, true);
  chdir(oldwd);

  free(oldwd);

  // find files in upload path
  for (auto &f : std::filesystem::directory_iterator(std::filesystem::path(ulpath))) {
    std::filesystem::path np(std::filesystem::absolute(get_curr_filebase()->uppath + "/" +f.path().filename().u8string()));
    if (std::filesystem::exists(np)) {
      bprintf("\r\n\r\n|12File Exists: |15%s\r\n", f.path().filename().u8string().c_str());
      continue;
    }
    
    bprintf("\r\n\r\n|10Found File: |15%s\r\n", f.path().filename().u8string().c_str());



    std::vector<std::string> description;

    bprintf("Please enter a description (5 lines max)");

    for (int i = 0; i < 5; i++) {
      bprintf("%d: ", i+1);
      std::string line = get_str(42);
      if (line.size() == 0) {
        break;
      }

      description.push_back(line);
    }

    if (description.size() == 0) {
      continue;
    }

    // copy file into upload
    if (!std::filesystem::copy_file(f.path(), np)) {
      bprintf("|12Copy file failed!|07\r\n");
      continue;
    }
    // add to database
    if (!get_curr_filebase()->insert_file(this, np.u8string(), description)) {
      bprintf("|12Failed to add file to database|07\r\n");
      continue;
    }
    bprintf("|10Thankyou for your upload!|07\r\n");
  }
  std::filesystem::remove_all(ulpath);
}
