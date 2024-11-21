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
    std::string daddr;
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

    MessageBase(std::string name, std::string file, std::string addr, std::string tagline, MsgBaseType t, int r, int w) {
        this->name = name;
        this->file = file;
        this->address = addr;
        this->tagline = tagline;
        this->mbtype = t;
        this->read_sec_level = r;
        this->write_sec_level = w;
    }
    
    std::string name;
    std::string file;
    std::string address;
    std::string tagline;

    int read_sec_level;
    int write_sec_level;

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
private:
    std::vector<std::string> demangle_ansi(Node *n, const char *msg, size_t len);
    std::string strip_ansi(std::string str);
};
