#pragma once

#include <string>
#include <sqlite3.h>
#include "Node.h"

class User {
public:
    enum InvalidUserReason {
        OK,
        TOOSHORT,
        TRASHCAN,
        INUSE,
        ERROR
    };

    static int check_password(Node *n, std::string username, std::string password);
    static void set_attrib(Node *n, std::string attrib, std::string value);
    static std::string get_attrib(Node *n, std::string attrib, std::string fallback);
    static std::string hash_sha256(std::string pass, std::string salt);
    static InvalidUserReason valid_username(Node *n, std::string username);
    static bool valid_fullname(Node *n, std::string fullname);
    static int inst_user(Node *n, std::string username, std::string password);
private:
    static bool open_database(std::string path, sqlite3 **db);
};