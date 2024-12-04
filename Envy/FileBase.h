#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <sqlite3.h>



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
    void inc_download(Node *n, std::string filename);
    bool insert_file(Node *n, std::string filename, std::vector<std::string> description);
    int count(Node *n);
private:
    bool open_database(std::string path, sqlite3 **db);
};

struct file_s {
  FileBase *fb;
  std::filesystem::path filename;
  std::string uploadedby;
  time_t uploaddate;
  int downloadcount;
  int64_t size;
  std::vector<std::string> desc;
};