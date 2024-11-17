#pragma once

#define VERSION "0.1"

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

class MessageBase;

class Node {
public:
    Node(int node, int socket, bool telnet);

    char getch();
    void cls();
    void putfile(std::string filename);
    bool putgfile(std::string gfile);
    std::string get_str(int length);
    std::string get_str(int length, char mask);
    void bprintf(const char *fmt, ...);
    int run();
    void disconnect();

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

    int get_uid() {
        return uid;
    }

    std::string get_username() {
        return username;
    }
    Logger *log;
private:

    bool detectANSI();

    void load_msgbases();

    std::vector<MessageBase *> msgbases;

    std::string username;

    std::string gfile_path;
    std::string data_path;
    std::string script_path;
    std::string log_path;
    std::string msg_path;

    bool ansi_supported;
    int term_width;
    int term_height;
    
    int uid;

    int curr_msgbase;

    int socket;
    int node;
    bool telnet;
    int tstage;
};
