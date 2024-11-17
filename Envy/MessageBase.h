#pragma once

#include <vector>
#include <string>

class Node;
class MessageBase {
public:
    MessageBase(std::string name, std::string file) {
        this->name = name;
        this->file = file;
    }
    std::string name;
    std::string file;
    void list_messages(Node *n, int startingat);
    void enter_message(Node *n);
    bool save_message(Node *n, std::string recipient, std::string subject, std::vector<std::string> msg);
    static time_t utc_to_local(time_t utc);
};
