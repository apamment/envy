#pragma once

#include <string>
#include <vector>
#include "Logger.h"

const unsigned char IAC = 255;
const unsigned char IAC_WILL = 251;
const unsigned char IAC_WONT = 252;
const unsigned char IAC_DO = 253;
const unsigned char IAC_DONT = 254;
const unsigned char IAC_TRANSMIT_BINARY = 0;
const unsigned char IAC_SUPPRESS_GO_AHEAD = 3;
const unsigned char IAC_ECHO = 1;

struct door_cfg_s {
    std::string name;
    std::string key;
    std::string script;
};

struct seclevel_s {
    std::string name;
    int level;
    int time_per_day;
    int timeout;
};

class MessageBase;

class Node {
public:
    
    Node(int node, int socket, bool telnet);

    char getch();
    void cls();
    void putfile(std::string filename);
    bool putgfile(std::string gfile);
    void launch_door(std::string key);
    std::string get_str(int length);
    std::string get_str(int length, char mask);
    std::string get_str(int length, char mask, std::string placeholder);
    void bprintf_nc(const char *fmt, ...);
    void bprintf(bool parse_pipes, const char *fmt, va_list args);
    void bprintf(const char *fmt, ...);
    int run();
    void disconnect();
    void pause();
    void select_msg_base();
    void scan_msg_bases();

    MessageBase *get_curr_msgbase();

    std::string get_script_path() {
        return script_path;
    }

    std::string get_data_path() {
        return data_path;
    }

    std::string get_msg_path() {
        return msg_path;
    }

    std::string get_tmp_path() {
        return tmp_path;
    }
    
    std::string get_gfile_path() {
        return gfile_path;
    }

    std::string get_bbsname() {
        return bbsname;
    }

    std::string get_opname() {
        return opname;
    }

    int get_socket() {
        return socket;
    }

    int get_uid() {
        return uid;
    }

    int get_term_height() {
        return term_height;
    }

    int get_term_width() {
        return term_width;
    }

    int get_node() {
        return node;
    }
    
    bool has_ansi() {
        return ansi_supported;
    }

    std::vector<struct door_cfg_s> get_doors() {
        return doors;
    }
    std::string get_username() {
        return username;
    }

    int get_timeleft() {
        return timeleft / 60;
    }

    int get_seclevel();

    Logger *log;
private:

    bool time_check();

    bool detectANSI();

    void load_msgbases();
    void load_doors();
    void load_seclevels();

    int get_timeperday();
    int get_timeout();

    std::vector<MessageBase *> msgbases;
    std::vector<MessageBase *> accessablemb;
    std::vector<struct door_cfg_s> doors;
    std::vector<struct seclevel_s> seclevels;

    std::string username;

    std::string gfile_path;
    std::string data_path;
    std::string script_path;
    std::string log_path;
    std::string msg_path;
    std::string tmp_path;

    std::string bbsname;
    std::string opname;

    std::string default_tagline;

    bool ansi_supported;
    int term_width;
    int term_height;
    
    int uid;

    int curr_msgbase;

    int socket;
    int node;
    bool telnet;
    int tstage;
    int timeleft;
    time_t last_time_check;
};
