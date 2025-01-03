extern "C" {
#include <jamlib/jam.h>
}
#include <string>
#include <cctype>
#include <ctime>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cstring>
#include "FullScreenEditor.h"
#include "MessageBase.h"
#include "Node.h"
#include "User.h"
#include "Version.h"

static inline void rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
}

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
  return save_message(n, recipient, n->get_username(), subject, msg, reply, daddress);
}

bool MessageBase::save_message(Node *n, std::string recipient, std::string sender, std::string subject, std::vector<std::string> msg, std::string reply,
                               std::string daddress) {
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
  jsf.DatLen = sender.length();
  jsf.Buffer = (uint8_t *)sender.c_str();
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

    kludge << std::to_string(high_msg + 1) << "." << file << "@" << address << " " << std::setw(8) << std::setfill('0') << std::setbase(16)
           << JAM_Crc32((uint8_t *)serialno.c_str(), serialno.length());

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
    body << "\r--- envy/" << VERSION << "-" << GITV << "\r * Origin: " << tagline << " (" << address << ")\r";
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
  if (mbtype == ECHO && n->get_echosem() != "") {
    std::ofstream sem(n->get_echosem(), std::ios_base::out | std::ios_base::trunc);
    if (sem.is_open()) {
      sem << time(NULL) << std::endl;
      sem.close();
    }
  } else if (mbtype == NETMAIL && n->get_netsem() != "") {
    std::ofstream sem(n->get_netsem(), std::ios_base::out | std::ios_base::trunc);
    if (sem.is_open()) {
      sem << time(NULL) << std::endl;
      sem.close();
    }
  }
  return true;
}

void MessageBase::enter_message(Node *n, std::string recipient, std::string subject, std::vector<std::string> *quotebuffer, struct msg_header_t *reply) {

  if (n->get_seclevel() < write_sec_level) {
    n->bprintf("\r\n\r\n|12Sorry, you don't have access to write in this area!\r\n\r\n");
    n->pause();
    return;
  }

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
      if (User::get_attrib(n, "signature-toggle", "off") == "on") {
        std::stringstream sigss(User::get_attrib(n, "signature", ""));
        std::string line;

        while(getline(sigss, line, '\r')) {
          msg.push_back(line);
        }
      }

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
        hdr.subject = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
      } else if (jsp->Fields[f]->LoID == JAMSFLD_SENDERNAME) {
        hdr.from = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
      } else if (jsp->Fields[f]->LoID == JAMSFLD_RECVRNAME) {
        hdr.to = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
      } else if (jsp->Fields[f]->LoID == JAMSFLD_MSGID) {
        hdr.msgid = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
      } else if (jsp->Fields[f]->LoID == JAMSFLD_OADDRESS) {
        hdr.oaddr = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
      } else if (jsp->Fields[f]->LoID == JAMSFLD_DADDRESS) {
        hdr.daddr = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
      }
    }

    bool ok = false;

    if (mbtype == NETMAIL) {
      if ((hdr.to == n->get_username() || hdr.to == User::get_attrib(n, "fullname", "UNKNOWN")) && hdr.daddr == address) {
        ok = true;
      } else if ((hdr.from == n->get_username() || hdr.from == User::get_attrib(n, "fullname", "UNKNOWN")) && hdr.oaddr == address) {
        ok = true;
      }
    } else {
      ok = true;
    }

    if (ok) {
      hdrs.push_back(hdr);
    }
    JAM_DelSubPacket(jsp);
  }

  JAM_CloseMB(jb);
  free(jb);

  int reading = startingat;
  while (true) {
    if (reading < 0 || reading >= hdrs.size()) {
      break;
    }
    char *body = (char *)malloc(hdrs.at(reading).body_len + 1);
    if (!body) {
      break;
    }
    memset(body, 0, hdrs.at(reading).body_len + 1);
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

    int ansimsg = false;

    for (size_t z = 0; z < hdrs.at(reading).body_len; z++) {
      if (body[z] == '\x1b') {
        ansimsg = true;
        break;
      }
    }

    if (ansimsg) {
      msg = demangle_ansi(n, body, hdrs.at(reading).body_len);
    } else {
      for (size_t i = 0; i < hdrs.at(reading).body_len; i++) {
        if (body[i] == '\r') {
          std::string line = ss.str();
          while (line.length() > 79) {
            std::string leftover;

            size_t pos = line.rfind(' ', 79);

            if (pos != std::string::npos) {
              leftover = line.substr(pos + 1);
              line = line.substr(0, pos);
            } else {
              leftover = line.substr(79);
              line = line.substr(0, 79);
            }

            msg.push_back(line + "\r\n");
            
            line = leftover;
          }

          msg.push_back(line + "\r\n");
          ss.str("");
          continue;
        }

        if (body[i] != '\n') {
          ss << body[i];
        }
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
    char ch = ' ';
    bool next = false;

    for (size_t i = 0; i < msg.size(); i++) {
      n->bprintf("%s", msg.at(i).c_str());
      lines++;
      if (lines == n->get_term_height() - 1 || i == msg.size() - 1) {
        if (i == msg.size() - 1) {
          n->bprintf("|11END  |15- |10(R)eply, (N)ext, (P)revious, (Q)uit: |07");
        } else {
          n->bprintf("|13MORE |15- |10(R)eply, (N)ext, (P)revious, (Q)uit: |07");
        }
        ch = n->getch();
        n->bprintf("\r\n");
        switch (tolower(ch)) {
        case 'n':
          reading = reading + 1;
          next = true;
          break;
        case 'p':
          reading = reading - 1;
          next = true;
          break;
        case 'r': {
          n->bprintf("\r\n");
          std::vector<std::string> qb;
          std::string leftover;
          for (size_t q = 0; q < msg.size(); q++) {
            std::string qbl = strip_ansi(msg.at(q));
            rtrim(qbl);
            if (qbl.length() >= 71) {
              while (qbl.length() >= 71) {
                size_t p = qbl.rfind(' ', 71);
                if (p != std::string::npos) {
                  leftover = qbl.substr(p + 1);
                  qb.push_back(" > " + qbl.substr(0, p));
                } else {
                  leftover = (qbl.substr(70));
                  qb.push_back(" > " + qbl.substr(0, 71));
                }

                q++;

                if (q < msg.size()) {
                  std::string nextqbl = strip_ansi(msg.at(q));
                  rtrim(nextqbl);
                  qbl = leftover + " " + nextqbl;
                  if (qbl.length() < 71) {
                    qb.push_back(" > " + qbl);                
                  }
                } else {
                  qb.push_back(" > " + leftover);
                  break;
                }          
              }
            } else {
              qb.push_back(" > " + qbl);
            }
          }

          enter_message(n, hdrs.at(reading).from, hdrs.at(reading).subject, &qb, &hdrs.at(reading));
          next = true;
        } break;
        case 'q':
          return;
        }      
        lines = 0;
        if (next == true) {
          break;
        }
      }
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
        hdr.subject = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
      } else if (jsp->Fields[f]->LoID == JAMSFLD_SENDERNAME) {
        hdr.from = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
      } else if (jsp->Fields[f]->LoID == JAMSFLD_RECVRNAME) {
        hdr.to = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
      } else if (jsp->Fields[f]->LoID == JAMSFLD_MSGID) {
        hdr.msgid = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
      } else if (jsp->Fields[f]->LoID == JAMSFLD_OADDRESS) {
        hdr.oaddr = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
      } else if (jsp->Fields[f]->LoID == JAMSFLD_DADDRESS) {
        hdr.daddr = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
      }
    }
    bool ok = false;

    if (mbtype == NETMAIL) {
      if ((hdr.to == n->get_username() || hdr.to == User::get_attrib(n, "fullname", "UNKNOWN")) && hdr.daddr == address) {
        ok = true;
      } else if ((hdr.from == n->get_username() || hdr.from == User::get_attrib(n, "fullname", "UNKNOWN")) && hdr.oaddr == address) {
        ok = true;
      }
    } else {
      ok = true;
    }

    if (ok) {
      hdrs.push_back(hdr);
    }
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
      n->bprintf("|08[|07%5d|08]|14*|15%-25.25s |10%-16.16s |11%-16.16s |13%04d/%02d/%02d|07\r\n", i + 1, hdrs.at(i).subject.c_str(), hdrs.at(i).to.c_str(),
                 hdrs.at(i).from.c_str(), dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday);
    } else {
      n->bprintf("|08[|07%5d|08] |15%-25.25s |10%-16.16s |11%-16.16s |13%04d/%02d/%02d|07\r\n", i + 1, hdrs.at(i).subject.c_str(), hdrs.at(i).to.c_str(),
                 hdrs.at(i).from.c_str(), dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday);
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

struct character_t {
  char c;
  int fg_color;
  int bg_color;
  bool bold;
};

std::vector<std::string> MessageBase::demangle_ansi(Node *n, const char *msg, size_t len) {
  return demangle_ansi(n, msg, len, n->get_term_width());
}

std::vector<std::string> MessageBase::demangle_ansi(Node *n, const char *msg, size_t len, int max_width) {
  std::vector<std::string> new_msg;
  int lines = 0;
  int line_at = 0;
  int col_at = 0;
  int params[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  int param_count = 0;
  int fg_color = 7;
  int bg_color = 0;
  bool bold = false;
  int save_col = 0;
  int save_row = 0;

  if (msg == NULL || len == 0) {
    return new_msg;
  }

  for (size_t i = 0; i < len; i++) {
    if (msg[i] == '\r' || (i >= 1 && msg[i] == '\n' && msg[i - 1] != '\r')) {
      line_at++;
      if (line_at > lines) {
        lines = line_at;
      }
      col_at = 0;
    } else if (msg[i] == '\x1b') {
      i++;
      if (msg[i] != '[') {
        i--;
        continue;
      } else {
        param_count = 0;
        while (i < len && strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", msg[i]) == NULL) {
          if (msg[i] == ';') {
            param_count++;
          } else if (msg[i] >= '0' && msg[i] <= '9') {
            if (param_count == 0) {
              param_count = 1;
              for (int j = 0; j < 9; j++) {
                params[j] = 0;
              }
            }
            params[param_count - 1] = params[param_count - 1] * 10 + (msg[i] - '0');
          }
          i++;
        }
        switch (msg[i]) {
        case 'A':
          if (param_count > 0) {
            line_at -= params[0];
          } else {
            line_at--;
          }
          if (line_at < 0)
            line_at = 0;
          break;
        case 'B':
          if (param_count > 0) {
            line_at += params[0];
          } else {
            line_at++;
          }
          if (line_at > lines) {
            lines = line_at;
          }
          break;
        case 'C':
          if (param_count > 0) {
            col_at += params[0];
          } else {
            col_at++;
          }
          if (col_at > (int)max_width) {
            col_at = max_width;
          }
          break;
        case 'D':
          if (param_count > 0) {
            col_at -= params[0];
          } else {
            col_at--;
          }
          if (col_at < 0)
            col_at = 0;
          break;
        case 'H':
        case 'f':
          if (param_count > 1) {
            params[0]--;
            params[1]--;
          }
          line_at = params[0];
          col_at = params[1];

          if (line_at > lines) {
            lines = line_at;
          }
          if (col_at > max_width) {
            col_at = max_width;
          }
          if (line_at < 0)
            line_at = 0;
          if (col_at < 0)
            col_at = 0;
          break;
        case 'u':
          col_at = save_col;
          line_at = save_row;
          break;
        case 's':
          save_col = col_at;
          save_row = line_at;
          break;
        case 'm':
          break;
        default:

          break;
        }
      }
    } else if (msg[i] != '\n') {
      col_at++;

      if (col_at >= max_width) {
        col_at = 0;
        line_at++;
        if (line_at > lines) {
          lines = line_at;
        }
      }
    }
  }

  struct character_t **fakescreen = (struct character_t **)malloc(sizeof(struct character_t *) * (lines + 1));

  if (!fakescreen) {
    return new_msg;
  }

  for (int i = 0; i <= lines; i++) {
    fakescreen[i] = (struct character_t *)malloc(sizeof(struct character_t) * (max_width + 1));
    if (!fakescreen[i]) {
      for (int j = i - 1; j >= 0; j--) {
        free(fakescreen[j]);
      }
      free(fakescreen);
      return new_msg;
    }
    for (size_t x = 0; x <= max_width; x++) {
      fakescreen[i][x].c = ' ';
      fakescreen[i][x].fg_color = 7;
      fakescreen[i][x].bg_color = 0;
    }
  }
  line_at = 0;
  col_at = 0;
  save_row = 0;
  save_col = 0;
  for (size_t i = 0; i < len; i++) {
    if (msg[i] == '\r' || (i >= 1 && msg[i] == '\n' && msg[i - 1] != '\r')) {
      line_at++;
      col_at = 0;
    } else if (msg[i] == '\x1b') {
      i++;
      if (msg[i] != '[') {
        i--;
        continue;
      } else {
        param_count = 0;
        while (i < len && strchr("ABCDEFGHIGJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", msg[i]) == NULL) {
          if (msg[i] == ';') {
            param_count++;
          } else if (msg[i] >= '0' && msg[i] <= '9') {
            if (param_count == 0) {
              param_count = 1;
              for (int j = 0; j < 9; j++) {
                params[j] = 0;
              }
            }
            params[param_count - 1] = params[param_count - 1] * 10 + (msg[i] - '0');
          }
          i++;
        }
        switch (msg[i]) {
        case 'A':
          if (param_count > 0) {
            line_at -= params[0];
          } else {
            line_at--;
          }
          if (line_at < 0)
            line_at = 0;
          break;
        case 'B':
          if (param_count > 0) {
            line_at += params[0];
          } else {
            line_at++;
          }
          break;
        case 'C':
          if (param_count > 0) {
            col_at += params[0];
          } else {
            col_at++;
          }
          if (col_at > max_width) {
            col_at = max_width;
          }
          break;
        case 'D':
          if (param_count > 0) {
            col_at -= params[0];
          } else {
            col_at--;
          }
          if (col_at < 0)
            col_at = 0;
          break;
        case 'H':
        case 'f':
          if (param_count > 1) {
            params[0]--;
            params[1]--;
          }
          line_at = params[0];
          col_at = params[1];
          if (line_at < 0)
            line_at = 0;
          if (col_at < 0)
            col_at = 0;
          if (col_at > max_width)
            col_at = max_width;
          break;
        case 'm':
          for (int z = 0; z < param_count; z++) {
            if (params[z] == 0) {
              bold = false;
              fg_color = 7;
              bg_color = 0;
            } else if (params[z] == 1) {
              bold = true;
            } else if (params[z] == 2) {
              bold = false;
            }

            else if (params[z] >= 30 && params[z] <= 37) {
              fg_color = params[z] - 30;
            } else if (params[z] >= 40 && params[z] <= 47) {
              bg_color = params[z] - 40;
            }
          }
          break;
        case 'u':
          col_at = save_col;
          line_at = save_row;
          break;
        case 's':
          save_col = col_at;
          save_row = line_at;
          break;
        }
      }
    } else if (msg[i] != '\n') {
      fakescreen[line_at][col_at].c = msg[i];
      fakescreen[line_at][col_at].bold = bold;
      fakescreen[line_at][col_at].fg_color = fg_color;
      fakescreen[line_at][col_at].bg_color = bg_color;
      col_at++;
      if (col_at >= max_width) {
        line_at++;
        col_at = 0;
      }
    }
  }

  for (int i = 0; i < lines; i++) {
    for (int j = max_width; j >= 0; j--) {
      if (fakescreen[i][j].c == ' ') {
        fakescreen[i][j].c = '\0';
      } else {
        break;
      }
    }
  }

  std::stringstream ss;

  fg_color = 7;
  bg_color = 0;
  bold = false;
  bool got_tearline = false;
  for (int i = 0; i < lines; i++) {
    ss.str("");
    size_t j;

    if ((fakescreen[i][0].c == '-' && fakescreen[i][1].c == '-' && fakescreen[i][2].c == '-') && (fakescreen[i][3].c == 0 || fakescreen[i][3].c == ' ')) {
      got_tearline = true;
      ss << "\x1b[0m";
    }

    if (!got_tearline) {
      if (fakescreen[i][0].c != '\001') {
        if (bold) {
          ss << "\x1b[1m";
        } else {
          ss << "\x1b[0m";
        }

        ss << "\x1b[" << std::to_string(fg_color + 30) << "m";
        ss << "\x1b[" << std::to_string(bg_color + 40) << "m";
      }
    }
    for (j = 0; j < max_width; j++) {
      if (fakescreen[i][j].c == '\0') {
        break;
      }
      if (!got_tearline) {
        bool reset = false;
        if (fakescreen[i][j].bold != bold) {
          bold = fakescreen[i][j].bold;
          if (bold) {
            ss << "\x1b[1m";
          } else {
            ss << "\x1b[0m";
            reset = true;
          }
        }

        if (fakescreen[i][j].fg_color != fg_color || reset) {
          fg_color = fakescreen[i][j].fg_color;
          ss << "\x1b[" << std::to_string(fg_color + 30) << "m";
        }
        if (fakescreen[i][j].bg_color != bg_color) {
          bg_color = fakescreen[i][j].bg_color;
          ss << "\x1b[" << std::to_string(bg_color + 40) << "m";
        }
      }
      ss << fakescreen[i][j].c;
    }
    if (j < max_width) {
      ss << "\r\n";
    }
    new_msg.push_back(ss.str());
  }

  for (int i = 0; i <= lines; i++) {
    free(fakescreen[i]);
  }
  free(fakescreen);

  return new_msg;
}

std::string MessageBase::strip_ansi(std::string str) {
  std::stringstream ss;

  for (size_t i = 0; i < str.size(); i++) {
    if (str.at(i) == '\x1b') {
      while (i < str.size() && std::string("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz").find(str.at(i)) == std::string::npos) {
        i++;
      }
      continue;
    }
    ss << str.at(i);
  }
  return ss.str();
}

struct msg_s *MessageBase::get_message(Node *n, int index) {
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
        return nullptr;
      }
    } else {
      return nullptr;
    }
  }

  JAM_ReadMBHeader(jb, &jbh);

  if (jbh.ActiveMsgs <= 0) {
    JAM_CloseMB(jb);
    free(jb);
    return nullptr;
  }

  int k = 0;

  for (size_t i = 0; i < jbh.ActiveMsgs; k++) {
    int ret = JAM_ReadMsgHeader(jb, k, &jmh, &jsp);
    if (ret != 0) {
      continue;
    }
    i++;

    if (jmh.MsgNum >= index) {

      struct msg_s *m = new struct msg_s;

      if (!m) {
        JAM_DelSubPacket(jsp);
        JAM_CloseMB(jb);
        free(jb);
        return nullptr;
      }

      m->id = jmh.MsgNum;
      for (size_t f = 0; f < jsp->NumFields; f++) {
        if (jsp->Fields[f]->LoID == JAMSFLD_SUBJECT) {
          m->subject = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
        } else if (jsp->Fields[f]->LoID == JAMSFLD_SENDERNAME) {
          m->from = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
        } else if (jsp->Fields[f]->LoID == JAMSFLD_RECVRNAME) {
          m->to = std::string((const char *)jsp->Fields[f]->Buffer, jsp->Fields[f]->DatLen);
        }
      }
      char *body = (char *)malloc(jmh.TxtLen + 1);
      if (!body) {
        break;
      }
      memset(body, 0, jmh.TxtLen + 1);
      JAM_ReadMsgText(jb, jmh.TxtOffset, jmh.TxtLen, (uint8_t *)body);

      m->body = std::string(body);

      free(body);
      JAM_DelSubPacket(jsp);
      JAM_CloseMB(jb);
      free(jb);
      return m;
    }
    JAM_DelSubPacket(jsp);
  }

  JAM_CloseMB(jb);
  free(jb);
  return nullptr;
}