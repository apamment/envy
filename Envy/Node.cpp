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
#include "Door.h"

Node::Node(int node, int socket, bool telnet) {
    this->node = node;
    this->socket = socket;
    this->telnet = telnet;
    this->ansi_supported = true;
    tstage = 0;
};

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
                if (buffer[i+1] >= '0' && buffer[i+1] <= '9' && buffer[i+2] >= '0' && buffer[i+2] <= '9') {
                    pipe_color = (buffer[i+1] - '0') * 10 + (buffer[i+2] - '0');

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
                                ss <<"0;35";
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
                    i+=2;
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
    return get_str(length, 0, "");
}


std::string Node::get_str(int length, char mask) {
    return get_str(length, mask, "");
}

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
    tmp_path = inir.Get("paths", "temp path", "tmp");
    default_tagline = inir.Get("main", "default tagline", "Unknown BBS");
    bbsname = inir.Get("main", "bbs name", "Unknown BBS");
    opname = inir.Get("main", "sysop name", "Unknown");

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

            uid = User::inst_user(this, username, password);

            if (uid == -1) {
                disconnect();
            } else {
                User::set_attrib(this, "fullname",  firstname + " " + lastname);
                User::set_attrib(this, "location", location);
                User::set_attrib(this, "email", email);
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

        struct tm timetm;
        localtime_r(&last_call, &timetm);
        if (timetm.tm_hour > 11) {
            bprintf("You last called on |15%s %d |07at |15%d:%02dpm|07\r\n", months[timetm.tm_mon], timetm.tm_mday, (timetm.tm_hour == 12 ? 12 : timetm.tm_hour - 12), timetm.tm_min);
        } else {
            bprintf("You last called on |15%s %d |07at |15%d:%02dam|07\r\n", months[timetm.tm_mon], timetm.tm_mday, (timetm.tm_hour == 0 ? 12 : timetm.tm_hour), timetm.tm_min);
        }
    }
    
    pause();

    Script::run(this, "menu");

    disconnect();

    return 0;
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
                msgbases.push_back(new MessageBase(myname, myfile, myoaddr, mytagline, mytype));
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

void Node::select_msg_base() {
    cls();
    
    int lines = 0;
    
    for (size_t i = 0; i < msgbases.size(); i++) {
        uint32_t ur = msgbases.at(i)->get_unread(this);
        uint32_t tot = msgbases.at(i)->get_total(this);
        bprintf("|07%4d. |15%-40.40s |10%d UNREAD |13%d TOTAL|07\r\n", i + 1, msgbases.at(i)->name.c_str(), ur, tot);
        if (lines == 22) {
            pause();
            lines = 0;
        }
    }

    bprintf("\r\nSelect area: ");
    std::string num = get_str(4);

    if (num.length() > 0) {
        try {
            int n = std::stoi(num);

            if (n - 1 >= 0 && n - 1 < msgbases.size()) {
                curr_msgbase = n - 1;
                User::set_attrib(this, "curr_mbase", msgbases.at(curr_msgbase)->file);
            }
        } catch (std::out_of_range const &) {
        } catch (std::invalid_argument const &) {
        }
    }
}

void Node::scan_msg_bases() {
    int lines = 0;

    cls();
    for (size_t i = 0; i < msgbases.size(); i++) {
        uint32_t ur = msgbases.at(i)->get_unread(this);
        uint32_t tot = msgbases.at(i)->get_total(this);

        bprintf("|07%4d. |15%-40.40s |10%d UNREAD |13%d TOTAL|07\r\n", i + 1, msgbases.at(i)->name.c_str(), ur, tot);
        lines++;
        if (lines == 22) {
            pause();
            lines = 0;
        }
    }
    pause();
}