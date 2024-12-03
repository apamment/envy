#pragma once

#include <string>

class Node;
class FileBase {
public:
    FileBase(std::string name, std::string uppath, std::string database, int down_sec_level, int up_sec_level) {
        this->name = name;
        this->uppath = uppath;
        this->database = database;
        this->down_sec_level = down_sec_level;
        this->up_sec_level = up_sec_level;
    }
    std::string name;
    std::string uppath;
    std::string database;
    int down_sec_level;
    int up_sec_level;

    void list_files(Node *n);
private:

};