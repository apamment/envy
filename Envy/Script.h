#pragma once

#include <string>
#include <sqlite3.h>
class Node;


class Script {
public:
    static bool open_database(Node *n, sqlite3 **db);    
    static int run(Node *n, std::string filename);
    static void set_attrib(Node *n, std::string script, std::string attrib, std::string value);
    static std::string get_attrib(Node *n, std::string script, std::string attrib, std::string fallback);
};
