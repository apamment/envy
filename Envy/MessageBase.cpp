extern "C" {
    #include <jamlib/jam.h>
}
#include <string>
#include <cctype>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cstring>
#include "FullScreenEditor.h"
#include "MessageBase.h"
#include "Node.h"



time_t MessageBase::utc_to_local(time_t utc) {
    time_t local;
    struct tm date_tm;

    localtime_r(&utc, &date_tm);

    local = utc + date_tm.tm_gmtoff;

    return local;
}

time_t MessageBase::gettz() {
	time_t offset;
	struct tm date_time;
	time_t utc = time(NULL);
	localtime_r(&utc, &date_time);
	offset = date_time.tm_gmtoff;
	return offset;
}


bool MessageBase::save_message(Node *n, std::string recipient, std::string subject, std::vector<std::string> msg, std::string reply, std::string daddress) {
    s_JamBase *jb;
    s_JamBaseHeader jbh;
    s_JamMsgHeader jmh;
    s_JamSubPacket *jsp;
    s_JamMsgHeader jmh2;
    s_JamSubPacket *jsp2;

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

    std::stringstream kludge;

    kludge << "CHRS: CP437 2";

    std::string chrs = kludge.str();

    jsf.LoID = JAMSFLD_FTSKLUDGE;
    jsf.HiID = 0;
    jsf.DatLen = chrs.length();
    jsf.Buffer = (uint8_t *)chrs.c_str();
    JAM_PutSubfield(jsp, &jsf);

    kludge.str("");

    time_t offset = gettz();
	int offhour = offset / 3600;
	int offmin = (offset % 3600) / 60;

    char buffer[16];

	if (offhour < 0) {
        snprintf(buffer, 16, "TZUTC: -%d%02d", abs(offhour), offmin);
	} else {
        snprintf(buffer, 16, "TZUTC: %d%02d", offhour, offmin);
	}

	jsf.LoID = JAMSFLD_FTSKLUDGE;
	jsf.HiID = 0;
	jsf.DatLen = strlen(buffer);
	jsf.Buffer = (uint8_t *)buffer;
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

    JAM_ReadMBHeader(jb, &jbh);

    ret = JAM_LockMB(jb, 100);
    if (ret != 0) {
        JAM_CloseMB(jb);
        free(jb);
        JAM_DelSubPacket(jsp);
        return false;
    }

    if (mbtype == ECHO) {
        // replyid and msgid
        if (reply != "") {
            jsf.LoID = JAMSFLD_REPLYID;
            jsf.HiID = 0;
            jsf.DatLen = reply.length();
            jsf.Buffer = (uint8_t *)reply.c_str();
            JAM_PutSubfield(jsp, &jsf);
        }
        kludge.str("");

        int k = 0;
        int high_msg = 0;

        for (size_t i = 0; i < jbh.ActiveMsgs; k++) {
            int ret = JAM_ReadMsgHeader(jb, k, &jmh2, &jsp2);
            if (ret != 0) {
                if (ret == JAM_NO_MESSAGE) {
                    continue;
                } else {
                    JAM_UnlockMB(jb);
                    JAM_CloseMB(jb);
                    free(jb);
                    return false;
                }
            }
            i++;

            if (high_msg < jmh2.MsgNum) {
                high_msg = jmh2.MsgNum;
            }
            JAM_DelSubPacket(jsp2);
        }


        std::string serialno = std::to_string(time(NULL)) + std::to_string(high_msg + 1);


        kludge << std::to_string(high_msg + 1) << "." << file << "@" << address <<  " " << std::setw(8) << std::setfill('0') << std::setbase(16) << JAM_Crc32((uint8_t *)serialno.c_str(), serialno.length());

        std::string msgid = kludge.str();

        jsf.LoID = JAMSFLD_MSGID;
        jsf.HiID = 0;
        jsf.DatLen = msgid.length();
        jsf.Buffer = (uint8_t *)msgid.c_str();
        JAM_PutSubfield(jsp, &jsf);

        jsf.LoID = JAMSFLD_OADDRESS;
        jsf.HiID = 0;
        jsf.DatLen = address.length();
        jsf.Buffer = (uint8_t *)address.c_str();
        JAM_PutSubfield(jsp, &jsf);
    } else if (mbtype == NETMAIL) {
        jsf.LoID = JAMSFLD_OADDRESS;
        jsf.HiID = 0;
        jsf.DatLen = address.length();
        jsf.Buffer = (uint8_t *)address.c_str();
        JAM_PutSubfield(jsp, &jsf);

        jsf.LoID = JAMSFLD_DADDRESS;
        jsf.HiID = 0;
        jsf.DatLen = daddress.length();
        jsf.Buffer = (uint8_t *)daddress.c_str();
        JAM_PutSubfield(jsp, &jsf);        
    }

    std::stringstream body;

    for (size_t i = 0; i < msg.size(); i++) {
        body << msg.at(i) << "\r";
    }

    if (mbtype == ECHO) {
        body << "\r--- envy\r * Origin: " << tagline << " (" << address << ")\r";
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

void MessageBase::enter_message(Node *n, std::string recipient, std::string subject, std::vector<std::string> *quotebuffer, struct msg_header_t *reply) {

    std::string daddress = "";

    n->bprintf("Recipient: ");
    recipient = n->get_str(16, 0, recipient);
    if (recipient.length() == 0) {
        recipient = "All";
    }
    
    n->bprintf("Subject: ");
    subject = n->get_str(32, 0, subject);

    if (subject.length() == 0) {
        n->bprintf("|12Aborted!|07\r\n");
        return;
    }

    if (mbtype == NETMAIL) {
        n->bprintf("Address: ");
        daddress = n->get_str(32, 0, (reply == nullptr ? "" : reply->oaddr));

        if (daddress.length() == 0) {
            // TODO: sanitize
            n->bprintf("|12Aborted!|07\r\n");
            return;
        }
    }

    int line = 1;

    std::string name;
    std::string file;  
    std::vector<std::string> msg;

    if (n->has_ansi()) {
        FullScreenEditor fse(n, recipient, subject, quotebuffer, nullptr);
        msg = fse.edit();
        if (msg.size() == 0) {
            n->bprintf("|12Aborted!|07\r\n");
            return;
        } else {
            if (!save_message(n, recipient, subject, msg, (reply == nullptr ? "" : reply->msgid), daddress)) {
                n->bprintf("|12Saving message failed.|07\r\n");
            } else {
                n->bprintf("|10Saved message!|07\r\n");
            }

            return;      
        }
    }

    while (true) {
        n->bprintf("|08[%05d]: |07", line);
        std::string line_str = n->get_str(70);
        if (line_str.length() >= 2 && line_str.at(0) == '/') {
            switch (tolower(line_str.at(1))) {
                case 's': {
                    if (!save_message(n, recipient, subject, msg, (reply == nullptr ? "" : reply->msgid), daddress)) {
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

    int k = 0;

    for (size_t i = 0; i < jbh.ActiveMsgs; k++) {
        struct msg_header_t hdr;
        int ret = JAM_ReadMsgHeader(jb, k, &jmh, &jsp);
        if (ret != 0) {
            continue;
        }
        i++;

	    hdr.date = jmh.DateWritten;
        hdr.body_len = jmh.TxtLen;
        hdr.body_off = jmh.TxtOffset;
        hdr.id = jmh.MsgNum;

        hdr.subject = "No Subject";
        hdr.to = "Unknown";
        hdr.from = "Unknown";
        hdr.msgid = "";

        for (size_t f = 0; f < jsp->NumFields; f++) {
            if (jsp->Fields[f]->LoID == JAMSFLD_SUBJECT) {
			    hdr.subject = std::string((const char*)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
		    } else if (jsp->Fields[f]->LoID == JAMSFLD_SENDERNAME) {
			    hdr.from = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
		    } else if (jsp->Fields[f]->LoID == JAMSFLD_RECVRNAME) {
                hdr.to = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
            } else if (jsp->Fields[f]->LoID == JAMSFLD_MSGID) {
                hdr.msgid = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
            } else if (jsp->Fields[f]->LoID == JAMSFLD_OADDRESS) {
                hdr.oaddr = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
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

        set_lastread(n, hdrs.at(reading).id);

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

            if (ss.str().length() == 75) {
                std::string line = ss.str();
                msg.push_back(line);
                ss.str("");
            }
        }

        free(body);
        n->cls();
        n->bprintf("|10Subj: |15%s\r\n", hdrs.at(reading).subject.c_str());
        n->bprintf("|10  To: |15%s\r\n", hdrs.at(reading).to.c_str());
        n->bprintf("|10From: |15%s\r\n", hdrs.at(reading).from.c_str());

        struct tm date_tm;

        gmtime_r(&hdrs.at(reading).date, &date_tm);

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

        n->bprintf("|10(R)eply, (N)ext, (P)revious, (Q)uit: |07");
        char ch = n->getch();

        switch(tolower(ch)) {
            case 'n':
                reading = reading + 1;
                break;
            case 'p':
                reading = reading - 1;
                break;
            case 'r':
                {

                    n->bprintf("\r\n");
                    std::vector<std::string> qb;

                    for (size_t q = 0; q < msg.size(); q++) {
                        qb.push_back(" > " + msg.at(q));
                    }

                    enter_message(n, hdrs.at(reading).from, hdrs.at(reading).subject, &qb, &hdrs.at(reading));
                }
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

    uint32_t hr = get_highread(n);

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

    int k = 0;

    for (size_t i = 0; i < jbh.ActiveMsgs; k++) {
        struct msg_header_t hdr;
        int ret = JAM_ReadMsgHeader(jb, k, &jmh, &jsp);
        if (ret != 0) {
            continue;
        }
        i++;
	    hdr.date = jmh.DateWritten;
        hdr.id = jmh.MsgNum;
        for (size_t f = 0; f < jsp->NumFields; f++) {
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


    n->cls();

    n->bprintf("|18|14 MSG NO SUBJECT                   TO               FROM             DATE       |07\r\n");
    int lines = 1;

    for (size_t i = startingat; i < hdrs.size(); i++) {

	    struct tm dt;

	    gmtime_r(&hdrs.at(i).date, &dt);
        if (hdrs.at(i).id > hr) {
            n->bprintf("|08[|07%5d|08]|14*|15%-25.25s |10%-16.16s |11%-16.16s |13%04d/%02d/%02d|07\r\n", i + 1, hdrs.at(i).subject.c_str(), hdrs.at(i).to.c_str(), hdrs.at(i).from.c_str(), dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday);
        } else {
            n->bprintf("|08[|07%5d|08] |15%-25.25s |10%-16.16s |11%-16.16s |13%04d/%02d/%02d|07\r\n", i + 1, hdrs.at(i).subject.c_str(), hdrs.at(i).to.c_str(), hdrs.at(i).from.c_str(), dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday);
        }
        lines++;
        if (lines == 23) {
            n->bprintf("|10Continue (Y/N) or Read MSG NO: |07");
            std::string ch = n->get_str(5);
            if (ch.length() > 0) {
                if (tolower(ch.at(0)) == 'n') {
                    return;
                } else {
                    try {
                        int num = std::stoi(ch);
                        read_messages(n, num - 1);
                        return;
                    } catch (std::out_of_range const &) {
                    } catch (std::invalid_argument const &) {
                    }
                }
            }
            n->cls();
            n->bprintf("|18|14 MSG NO SUBJECT                   TO               FROM             DATE       |07\r\n");
            lines = 1;
        }
    }
    
    n->bprintf("|10Read MSG NO:|07");

    std::string ch = n->get_str(5);
    if (ch.length() > 0) {
        try {
            int num = std::stoi(ch);
            read_messages(n, num - 1);
        } catch (std::out_of_range const &) {
        } catch (std::invalid_argument const &) {
        }
    }
}

void MessageBase::set_lastread(Node *n, uint32_t msgid) {
    s_JamBase *jb;
    s_JamLastRead jlr;

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

    ret = JAM_ReadLastRead(jb, n->get_uid(), &jlr);

    if (ret != 0) {
        if (ret == JAM_NO_USER) {
            jlr.HighReadMsg = msgid;
            jlr.LastReadMsg = msgid;
            char *buffer = (char *)malloc(n->get_username().length() + 1);
            for (size_t i = 0; i < n->get_username().length(); i++) {
                buffer[i] = tolower(n->get_username().at(i));
                buffer[i + 1] = '\0';
            }

            jlr.UserCRC = JAM_Crc32((uint8_t *)buffer, n->get_username().length());
            jlr.UserID = n->get_uid();
            free(buffer);
            JAM_WriteLastRead(jb, n->get_uid(), &jlr);

            JAM_CloseMB(jb);
            free(jb);

            return;
        } else {
            JAM_CloseMB(jb);
            free(jb);
            return;
        }
    }

    if (jlr.HighReadMsg < msgid) {
        jlr.HighReadMsg = msgid;
    }
    jlr.LastReadMsg = msgid;
    JAM_WriteLastRead(jb, n->get_uid(), &jlr);

    JAM_CloseMB(jb);
    free(jb);
    return;
}

uint32_t MessageBase::get_lastread(Node *n) {
    s_JamBase *jb;
    s_JamLastRead jlr;

    int ret;
    ret = JAM_OpenMB((uint8_t *)std::string(n->get_msg_path() + "/" + file).c_str(), &jb);
    if (ret != 0) {
        free(jb);
        if (ret == JAM_IO_ERROR) {
            ret = JAM_CreateMB((uint8_t *)std::string(n->get_msg_path() + "/" + file).c_str(), 1, &jb);
            if (ret != 0) {
                free(jb);
                return 0;
            }
        } else {
            return 0;
        }
    }

    ret = JAM_ReadLastRead(jb, n->get_uid(), &jlr);

    if (ret != 0) {
        JAM_CloseMB(jb);
        free(jb);
        return 0;
    } else {
        uint32_t retval = jlr.LastReadMsg;
        JAM_CloseMB(jb);
        free(jb);
        return retval;
    }
}

uint32_t MessageBase::get_highread(Node *n) {
    s_JamBase *jb;
    s_JamLastRead jlr;

    int ret;
    ret = JAM_OpenMB((uint8_t *)std::string(n->get_msg_path() + "/" + file).c_str(), &jb);
    if (ret != 0) {
        free(jb);
        if (ret == JAM_IO_ERROR) {
            ret = JAM_CreateMB((uint8_t *)std::string(n->get_msg_path() + "/" + file).c_str(), 1, &jb);
            if (ret != 0) {
                free(jb);
                return 0;
            }
        } else {
            return 0;
        }
    }

    ret = JAM_ReadLastRead(jb, n->get_uid(), &jlr);

    if (ret != 0) {
        JAM_CloseMB(jb);
        free(jb);
        return 0;
    } else {
        uint32_t retval = jlr.HighReadMsg;
        JAM_CloseMB(jb);
        free(jb);
        return retval;
    }
}

uint32_t MessageBase::get_total(Node *n) {
    s_JamBase *jb;
    s_JamBaseHeader jbh;
    int ret;
    ret = JAM_OpenMB((uint8_t *)std::string(n->get_msg_path() + "/" + file).c_str(), &jb);
    if (ret != 0) {
        free(jb);
        if (ret == JAM_IO_ERROR) {
            ret = JAM_CreateMB((uint8_t *)std::string(n->get_msg_path() + "/" + file).c_str(), 1, &jb);
            if (ret != 0) {
                free(jb);
                return 0;
            }
        } else {
            return 0;
        }
    }

    JAM_ReadMBHeader(jb, &jbh);

    ret = jbh.ActiveMsgs;

    JAM_CloseMB(jb);
    free(jb);

    return ret;
}

uint32_t MessageBase::get_unread(Node *n) {
    uint32_t hr = get_highread(n);
    uint32_t unread = 0;
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
                return 0;
            }
        } else {
            return 0;
        }
    }

    JAM_ReadMBHeader(jb, &jbh);

    if (jbh.ActiveMsgs <= 0) {
        JAM_CloseMB(jb);
        free(jb);
        return 0;
    }

    int k = 0;

    for (size_t i = 0; i < jbh.ActiveMsgs; k++) {
        struct msg_header_t hdr;
        int ret = JAM_ReadMsgHeader(jb, k, &jmh, &jsp);
        if (ret != 0) {
            continue;
        }
        if (jmh.MsgNum > hr) {
            unread++;
        }
        JAM_DelSubPacket(jsp);
        i++;
    }
    JAM_CloseMB(jb);
    free(jb);

    return unread;
}