#pragma once

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
};
