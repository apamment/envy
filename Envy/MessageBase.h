#pragma once

#include <vector>
#include <string>

struct msg_header_t {
    uint32_t id;
    std::string from;
    std::string to;
    std::string subject;
    std::string msgid;
    std::string oaddr;
    time_t date;
    size_t body_len;
    size_t body_off;
};

class Node;
class MessageBase {
public:
    enum MsgBaseType {
        LOCAL,
        ECHO,
        NETMAIL
    };

    MessageBase(std::string name, std::string file, std::string addr, std::string tagline, MsgBaseType t) {
        this->name = name;
        this->file = file;
        this->address = addr;
        this->tagline = tagline;
        this->mbtype = t;
    }
    
    std::string name;
    std::string file;
    std::string address;
    std::string tagline;

    MsgBaseType mbtype;

    void set_lastread(Node *n, uint32_t msgno);
    uint32_t get_lastread(Node *n);
    uint32_t get_highread(Node *n);
    void read_messages(Node *n, int startingat);
    void list_messages(Node *n, int startingat);
    void enter_message(Node *n, std::string recpient, std::string subject, std::vector<std::string> *quotebuffer, struct msg_header_t *reply);
    bool save_message(Node *n, std::string recipient, std::string subject, std::vector<std::string> msg, std::string reply, std::string daddress);
    static time_t utc_to_local(time_t utc);
    static time_t gettz();
    uint32_t get_unread(Node *n);
    uint32_t get_total(Node *n);
};
