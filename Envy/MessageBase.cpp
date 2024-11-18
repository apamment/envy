extern "C" {
    #include <jamlib/jam.h>
}
#include <string>
#include <cctype>
#include <ctime>
#include <sstream>
#include "MessageBase.h"
#include "Node.h"

struct msg_header_t {
    uint32_t id;
    std::string from;
    std::string to;
    std::string subject;
    time_t date;
    size_t body_len;
    size_t body_off;
};

time_t MessageBase::utc_to_local(time_t utc) {
    time_t local;
    struct tm date_tm;

    localtime_r(&utc, &date_tm);

    local = utc + date_tm.tm_gmtoff;

    return local;
}

bool MessageBase::save_message(Node *n, std::string recipient, std::string subject, std::vector<std::string> msg) {
    s_JamBase *jb;
    s_JamMsgHeader jmh;
    s_JamSubPacket *jsp;
    s_JamSubfield jsf;

    JAM_ClearMsgHeader(&jmh);

    jmh.DateWritten = utc_to_local(time(NULL));
    jmh.Attribute |= MSG_LOCAL;

    jsp = JAM_NewSubPacket();
    jsf.LoID = JAMSFLD_SENDERNAME;
    jsf.HiID = 0;
    jsf.DatLen = n->get_username().length();
    jsf.Buffer = (uint8_t *)n->get_username().c_str();
    JAM_PutSubfield(jsp, &jsf);

    jsf.LoID = JAMSFLD_RECVRNAME;
    jsf.HiID = 0;
    jsf.DatLen = recipient.length();
    jsf.Buffer = (uint8_t *)recipient.c_str();
    JAM_PutSubfield(jsp, &jsf);

    jsf.LoID = JAMSFLD_SUBJECT;
    jsf.HiID = 0;
    jsf.DatLen = subject.length();
    jsf.Buffer = (uint8_t *)subject.c_str();
    JAM_PutSubfield(jsp, &jsf);

    int ret;
    ret = JAM_OpenMB((uint8_t *)std::string(n->get_msg_path() + "/" + file).c_str(), &jb);
    if (ret != 0) {
        free(jb);
        if (ret == JAM_IO_ERROR) {
            ret = JAM_CreateMB((uint8_t *)std::string(n->get_msg_path() + "/" + file).c_str(), 1, &jb);
            if (ret != 0) {
                free(jb);
                return false;
            }
        } else {
            return false;
        }
    }

    ret = JAM_LockMB(jb, 100);
    if (ret != 0) {
        JAM_CloseMB(jb);
        free(jb);
        JAM_DelSubPacket(jsp);
        return false;
    }

    std::stringstream body;

    for (size_t i = 0; i < msg.size(); i++) {
        body << msg.at(i) << "\r";
    }

    if (JAM_AddMessage(jb, &jmh, jsp, (uint8_t *)body.str().c_str(), body.str().length())) {
        // failed to add message
        JAM_UnlockMB(jb);
        JAM_CloseMB(jb);
        free(jb);
        JAM_DelSubPacket(jsp);
        return false;
    }

    JAM_UnlockMB(jb);
    JAM_CloseMB(jb);
    free(jb);
    JAM_DelSubPacket(jsp);
    return true;
}

void MessageBase::enter_message(Node *n) {
    n->bprintf("Recipient: ");
    std::string recipient = n->get_str(16);
    if (recipient.length() == 0) {
        recipient = "All";
    }
    n->bprintf("Subject: ");
    std::string subject = n->get_str(32);

    if (subject.length() == 0) {
        n->bprintf("|12Aborted!|07\r\n");
        return;
    }

    int line = 1;

    std::string name;
    std::string file;  
    std::vector<std::string> msg;

    while (true) {
        n->bprintf("[%05d]: ", line);
        std::string line_str = n->get_str(70);
        if (line_str.length() >= 2 && line_str.at(0) == '/') {
            switch (tolower(line_str.at(1))) {
                case 's': {
                    if (!save_message(n, recipient, subject, msg)) {
                        n->bprintf("|12Saving message failed.|07\r\n");
                    } else {
                        n->bprintf("|10Saved message!|07\r\n");
                    }
                }
                return;
                case 'a': 
                    n->bprintf("|12Aborted!|07\r\n");
                return;
                default:
                break;
                
            } 
        } else {
            msg.push_back(line_str);
            line++;
        }
    }
}

void MessageBase::read_messages(Node *n, int startingat) {
    std::vector<struct msg_header_t> hdrs;
    
    s_JamBase *jb;
    s_JamBaseHeader jbh;
    s_JamMsgHeader jmh;
    s_JamSubPacket *jsp;
    int ret;
    ret = JAM_OpenMB((uint8_t *)std::string(n->get_msg_path() + "/" + file).c_str(), &jb);
    if (ret != 0) {
        free(jb);
        if (ret == JAM_IO_ERROR) {
            ret = JAM_CreateMB((uint8_t *)std::string(n->get_msg_path() + "/" + file).c_str(), 1, &jb);
            if (ret != 0) {
                free(jb);
                return;
            }
        } else {
            return;
        }
    }
JAM_ReadMBHeader(jb, &jbh);

    if (jbh.ActiveMsgs <= 0) {
        JAM_CloseMB(jb);
        free(jb);
        n->bprintf("|14No messages in message base..,|07\r\n");
        n->pause();
        return;
    }

    for (size_t i = 0; i < jbh.ActiveMsgs; i++) {
        struct msg_header_t hdr;
        int ret = JAM_ReadMsgHeader(jb, i, &jmh, &jsp);
        if (ret != 0) {
            continue;
        }
    
	    hdr.date = jmh.DateWritten;
        hdr.body_len = jmh.TxtLen;
        hdr.body_off = jmh.TxtOffset;
        for (size_t f = 0; f < jsp->NumFields; f++) {
            hdr.id = i;
            if (jsp->Fields[f]->LoID == JAMSFLD_SUBJECT) {
			    hdr.subject = std::string((const char*)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
		    } else if (jsp->Fields[f]->LoID == JAMSFLD_SENDERNAME) {
			    hdr.from = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
		    } else if (jsp->Fields[f]->LoID == JAMSFLD_RECVRNAME) {
                hdr.to = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
            }
        }

        hdrs.push_back(hdr);
        JAM_DelSubPacket(jsp);
    }

    JAM_CloseMB(jb);
    free(jb);    

    int reading = startingat;
    while(true) {
        if (reading < 0 || reading >= hdrs.size()) {
            break;
        }
        char *body = (char *)malloc(hdrs.at(reading).body_len + 1);
        if (!body) {
            break;
        }
        ret = JAM_OpenMB((uint8_t *)std::string(n->get_msg_path() + "/" + file).c_str(), &jb);
        if (ret != 0) {
            free(body);
            free(jb);
            break;
        }
        
        JAM_ReadMsgText(jb, hdrs.at(reading).body_off, hdrs.at(reading).body_len, (uint8_t *)body);
        JAM_CloseMB(jb);
        free(jb);
        std::stringstream ss;
        std::vector<std::string> msg;

        for (size_t i = 0; i < hdrs.at(reading).body_len; i++) {
            if (body[i] == '\r') {
                std::string line = ss.str();
                msg.push_back(line);
                ss.str("");
                continue;
            } 
            ss << body[i];

            if (ss.str().length() == 79) {
                std::string line = ss.str();
                msg.push_back(line);
                ss.str("");
            }
        }

        free(body);
        n->cls();
        n->bprintf("|10Subj: |15%s\r\n", hdrs.at(reading).subject.c_str());
        n->bprintf("|10  To: |15%s\r\n", hdrs.at(reading).from.c_str());
        n->bprintf("|10From: |15%s\r\n", hdrs.at(reading).from.c_str());

        struct tm date_tm;

        localtime_r(&hdrs.at(reading).date, &date_tm);

        n->bprintf("|10Date: |15%02d:%02d %02d-%02d-%04d\r\n", date_tm.tm_hour, date_tm.tm_min, date_tm.tm_mday, date_tm.tm_mon + 1, date_tm.tm_year + 1900);
        n->bprintf("|08-------------------------------------------------------------------------------|07\r\n");

        int lines = 5;

        for (size_t i = 0; i < msg.size(); i++) {
            n->bprintf("%s\r\n", msg.at(i).c_str());
            lines++;
            if (lines == 23) {
                n->pause();
                lines = 0;
            }
        }

        n->bprintf("|10(N)ext, (P)revious, (Q)uit: |07");
        char ch = n->getch();

        switch(tolower(ch)) {
            case 'n':
                reading = reading + 1;
                break;
            case 'p':
                reading = reading - 1;
                break;
            case 'q':
                return;
        }
    }
}

void MessageBase::list_messages(Node *n, int startingat) {
    std::vector<struct msg_header_t> hdrs;
    
    s_JamBase *jb;
    s_JamBaseHeader jbh;
    s_JamMsgHeader jmh;
    s_JamSubPacket *jsp;
    int ret;
    ret = JAM_OpenMB((uint8_t *)std::string(n->get_msg_path() + "/" + file).c_str(), &jb);
    if (ret != 0) {
        free(jb);
        if (ret == JAM_IO_ERROR) {
            ret = JAM_CreateMB((uint8_t *)std::string(n->get_msg_path() + "/" + file).c_str(), 1, &jb);
            if (ret != 0) {
                free(jb);
                return;
            }
        } else {
            return;
        }
    }

    JAM_ReadMBHeader(jb, &jbh);

    if (jbh.ActiveMsgs <= 0) {
        JAM_CloseMB(jb);
        free(jb);
        n->bprintf("|14No messages in message base..,|07\r\n");
        n->pause();
        return;
    }

    for (size_t i = 0; i < jbh.ActiveMsgs; i++) {
        struct msg_header_t hdr;
        int ret = JAM_ReadMsgHeader(jb, i, &jmh, &jsp);
        if (ret != 0) {
            continue;
        }
    
	    hdr.date = jmh.DateWritten;
    
        for (size_t f = 0; f < jsp->NumFields; f++) {
            hdr.id = i;
            if (jsp->Fields[f]->LoID == JAMSFLD_SUBJECT) {
			    hdr.subject = std::string((const char*)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
		    } else if (jsp->Fields[f]->LoID == JAMSFLD_SENDERNAME) {
			    hdr.from = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
		    } else if (jsp->Fields[f]->LoID == JAMSFLD_RECVRNAME) {
                hdr.to = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
            }
        }

        hdrs.push_back(hdr);
        JAM_DelSubPacket(jsp);
    }

    JAM_CloseMB(jb);
    free(jb);


    int lines = 0;

    for (size_t i = startingat; i < hdrs.size(); i++) {

	    struct tm dt;

	    gmtime_r(&hdrs.at(i).date, &dt);

        n->bprintf("|08[|07%5d|08] |15%-25.25s |10%-16.16s |11%-16.16s |13%04d/%02d/%02d|07\r\n", i + 1, hdrs.at(i).subject.c_str(), hdrs.at(i).to.c_str(), hdrs.at(i).from.c_str(), dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday);
        lines++;
        if (lines == 23) {
            n->bprintf("Continue (Y/N)?");
            char ch = n->getch();
            if (tolower(ch) == 'n') {
                return;
            }
            n->bprintf("\r\n");
            lines = 0;
        }
    }

    n->pause();
}
