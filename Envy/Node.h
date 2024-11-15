#pragma once

#define VERSION "0.1"

#include <string>

const unsigned char IAC = 255;
const unsigned char IAC_WILL = 251;
const unsigned char IAC_WONT = 252;
const unsigned char IAC_DO = 253;
const unsigned char IAC_DONT = 254;
const unsigned char IAC_TRANSMIT_BINARY = 0;
const unsigned char IAC_SUPPRESS_GO_AHEAD = 3;
const unsigned char IAC_ECHO = 1;


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

    std::string get_data_path() {
        return data_path;
    }

    int get_uid() {
        return uid;
    }

private:

    bool detectANSI();

    std::string username;

    std::string gfile_path;
    std::string data_path;
    std::string script_path;



    bool ansi_supported;
    int term_width;
    int term_height;
    
    int uid;

    int socket;
    int node;
    bool telnet;
    int tstage;
};
