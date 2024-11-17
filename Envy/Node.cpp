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
#include "Disconnect.h"
#include "Script.h"
#include "User.h"

Node::Node(int node, int socket, bool telnet) {
    this->node = node;
    this->socket = socket;
    this->telnet = telnet;
    this->ansi_supported = true;
    tstage = 0;
};

void Node::disconnect() {
    close(socket);

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



void Node::bprintf(const char *fmt, ...) {
    char buffer[2048];
    va_list args;

    va_start(args, fmt);

    vsnprintf(buffer, sizeof buffer, fmt, args);
    send(socket, buffer, strlen(buffer), 0);

    va_end(args);
}

void Node::putfile(std::string filename) {
    std::ifstream t(filename);
    char ch;

    if (t.is_open()) {
        while (!t.eof()) {
            t.get(ch);
            if (ch == 0x1a) break;
            if (ch != '\n') {
                send(socket, &ch, 1, 0);
            }
            if (ch == '\r') {
                send(socket, "\r\n", 2, 0);
            }
        }

        if (ch != '\r' && ch != '\n') {
            send(socket, "\r\n", 2, 0);
        }

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


char Node::getch() {
    unsigned char c;
    unsigned char dowillwontdont = 0;

    while(true) {
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
                } else if (c == 250){
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

std::string Node::get_str(int length) {
    return get_str(length, 0);
}

std::string Node::get_str(int length, char mask) {
    std::stringstream ss;

    if (ansi_supported) {
        bprintf("\033[s\033[1;37;42m");

        for (int i = 0; i < length; i++) {
            bprintf(" ");
        }

        bprintf("\033[u\033[1;37;42m");
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

    unsigned char iac_echo[] = {IAC, IAC_WILL, IAC_ECHO, '\0'};
    unsigned char iac_sga[] = {IAC, IAC_WILL, IAC_SUPPRESS_GO_AHEAD, '\0'};

    INIReader inir("envy.ini");
    if (inir.ParseError() != 0) {
        return -1;
    }

    // read config
    gfile_path = inir.Get("paths", "gfile path", "gfiles");
    data_path = inir.Get("paths", "data path", "data");
    script_path = inir.Get("paths", "scripts path", "scripts");
    log_path = inir.Get("paths", "log path", "logs");
    msg_path = inir.Get("paths", "message path", "msgs");

    log = new Logger();
    log->load(log_path + "/envy." + std::to_string(node) + ".log");

    load_msgbases();

    log->log(LOG_INFO, "Connected!");

    // send initial telnet negotiation
    send(socket, (char *)iac_echo, 3, 0);
    send(socket, (char *)iac_sga, 3, 0);

    bprintf("Envy %s - Copyright (C) 2024; Andrew Pamment\r\n", VERSION);

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
                    switch(reason) {
                        case User::InvalidUserReason::ERROR:
                            bprintf("\r\nSorry, an error occured.\r\n");
                            break;
                        case User::InvalidUserReason::TOOSHORT:
                            bprintf("\r\nSorry, that username is too short.\r\n");
                            break;
                        case User::InvalidUserReason::INUSE:
                            bprintf("\r\nSorry, that username is in use.\r\n");
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

            while(true) {
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

            uid = User::inst_user(this, username, password, firstname + " " + lastname, location, email);

            if (uid == -1) {
                disconnect();
            } else {
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

    curr_msgbase = -1;
    std::string cmb = User::get_attrib(this, "curr_mbase", "");

    if (cmb != "") {
        for (size_t i = 0; i < msgbases.size(); i++) {
            if (msgbases.at(i)->file == cmb) {
                curr_msgbase = i;
                break;
            }
        }
    }
    if (curr_msgbase == -1) {
        if (msgbases.size() > 0) {
            curr_msgbase = 0;
        }
    }

    time_t last_call = std::stoi(User::get_attrib(this, "last-call", "0"));
    int total_calls = std::stoi(User::get_attrib(this, "total-calls", "0"));

    total_calls++;

    User::set_attrib(this, "total-calls", std::to_string(total_calls));
    User::set_attrib(this, "last-call", std::to_string(time(NULL)));

    cls();

    if (total_calls == 1) {
        bprintf("Welcome %s, this is your first call!\r\n", username.c_str());
    } else {
        if (total_calls % 10 == 1 && total_calls % 100 != 11) {
            bprintf("Welcome back %s, this is your %dst call!\r\n", username.c_str(), total_calls);
        } else if (total_calls % 10 == 2 && total_calls % 100 != 12) {
            bprintf("Welcome back %s, this is your %dnd call!\r\n", username.c_str(), total_calls);
        } else if (total_calls % 10 == 3 && total_calls % 100 != 13) {
            bprintf("Welcome back %s, this is your %drd call!\r\n", username.c_str(), total_calls);
        } else {
            bprintf("Welcome back %s, this is your %dth call!\r\n", username.c_str(), total_calls);
        }

        struct tm timetm;
        localtime_r(&last_call, &timetm);
        if (timetm.tm_hour > 11) {
            bprintf("You last called on %s %d at %d:%02dpm\r\n", months[timetm.tm_mon], timetm.tm_mday, (timetm.tm_hour == 12 ? 12 : timetm.tm_hour - 12), timetm.tm_min);
        } else {
            bprintf("You last called on %s %d at %d:%02dam\r\n", months[timetm.tm_mon], timetm.tm_mday, (timetm.tm_hour == 0 ? 12 : timetm.tm_hour), timetm.tm_min);
        }
    }

    bprintf("Press a key...");
    getch();

    Script::run(this, "menu");

    disconnect();

    return 0;
}

void Node::load_msgbases() {
    try {
        auto data = toml::parse_file(data_path + "/msgbases.toml");
        auto baseitems = data.get_as<toml::array>("msgbase");

        for (size_t i = 0; i < baseitems->size(); i++) {
            auto itemtable = baseitems->get(i)->as_table();

            std::string myname;
            std::string myfile;

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
            if (myfile != "") {
                msgbases.push_back(new MessageBase(myname, myfile));
            }
        }

    } catch (toml::parse_error const &p) {
        log->log(LOG_ERROR, "Error parsing %s/msgbases.toml, Line %d, Column %d", data_path.c_str(), p.source().begin.line, p.source().begin.column);
        log->log(LOG_ERROR, " -> %s", std::string(p.description()).c_str());
    }
}

MessageBase *Node::get_curr_msgbase() {
    if (curr_msgbase != -1) {
        return msgbases.at(curr_msgbase);
    }

    return nullptr;
}
