#include "Email.h"
#include "Node.h"
#include "FullScreenEditor.h"
#include <cstring>
#include <iostream>
#include <sqlite3.h>
#include <sstream>
#include <string>
#include <fstream>

bool Email::open_database(std::string filename, sqlite3 **db) {
  const char *create_users_sql = "CREATE TABLE IF NOT EXISTS email(id INTEGER PRIMARY KEY, sender TEXT COLLATE NOCASE, recipient TEXT COLLATE NOCASE, subject "
                                 "TEXT, body TEXT, date INTEGER, seen INTEGER)";

  int rc;
  char *err_msg = NULL;

  if (sqlite3_open(filename.c_str(), db) != SQLITE_OK) {
    std::cerr << "Unable to open database: " << filename << std::endl;
    return false;
  }
  sqlite3_busy_timeout(*db, 5000);

  rc = sqlite3_exec(*db, create_users_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    std::cerr << "Unable to create email table: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }
  return true;
}

int Email::unread_email(Node *n) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  static const char sql[] = "SELECT COUNT(*) FROM email WHERE recipient=? AND seen=\"0\"";

  if (!open_database(n->get_data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare unread_email (email) sql");
    sqlite3_close(db);
    return 0;
  }
  std::string uname = n->get_username();

  sqlite3_bind_text(stmt, 1, uname.c_str(), -1, NULL);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    int result = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return 0;
}

int Email::count_email(Node *n) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  static const char sql[] = "SELECT COUNT(*) FROM email WHERE recipient=?";

  if (!open_database(n->get_data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare count_email (email) sql");
    sqlite3_close(db);
    return 0;
  }
  std::string uname = n->get_username();

  sqlite3_bind_text(stmt, 1, uname.c_str(), -1, NULL);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    int result = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return result;
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return 0;
}

void Email::enter_message(Node *n, std::string recipient, std::string subject, std::vector<std::string> *quotebuffer) {

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
      if (!save_message(n, recipient, n->get_username(), subject, msg)) {
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
        if (!save_message(n, recipient, n->get_username(), subject, msg)) {
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

bool Email::save_message(Node *n, std::string to, std::string from, std::string subject, std::vector<std::string> msg) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  static const char sql[] = "INSERT INTO email (sender, recipient, subject, body, date, seen) VALUES(?, ?, ?, ?, ?, 0)";

  if (!open_database(n->get_data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return false;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare save_message (email) sql");
    sqlite3_close(db);
    return false;
  }

  std::stringstream ss;

  for (size_t i = 0; i < msg.size(); i++) {
    ss << msg.at(i) << "\r";
  }

  std::string msgstr = ss.str();

  time_t now = time(NULL);

  sqlite3_bind_text(stmt, 1, from.c_str(), -1, NULL);
  sqlite3_bind_text(stmt, 2, to.c_str(), -1, NULL);
  sqlite3_bind_text(stmt, 3, subject.c_str(), -1, NULL);
  sqlite3_bind_text(stmt, 4, msgstr.c_str(), -1, NULL);
  sqlite3_bind_int64(stmt, 5, now);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return true;
}

bool Email::load_emails(Node *n, std::vector<Email> *emails) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  static const char sql[] = "SELECT id, sender, subject, body, date, seen FROM email WHERE recipient = ?";

  if (!open_database(n->get_data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return false;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare email sqlite query");
    sqlite3_close(db);
    return false;
  }
  std::string uname = n->get_username();
  sqlite3_bind_text(stmt, 1, uname.c_str(), -1, NULL);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    Email e;
    std::string body;
    std::stringstream ss;

    e.id = sqlite3_column_int(stmt, 0);
    e.sender = std::string((const char *)sqlite3_column_text(stmt, 1));
    e.subject = std::string((const char *)sqlite3_column_text(stmt, 2));
    body = std::string((const char *)sqlite3_column_text(stmt, 3));
    e.date = sqlite3_column_int64(stmt, 4);
    e.seen = (sqlite3_column_int(stmt, 5) == 1);

    for (size_t i = 0; i < body.size(); i++) {
      if (body[i] == '\r') {
        e.msg.push_back(ss.str());
        ss.str("");
      } else {
        ss << body[i];
      }
    }

    emails->push_back(e);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return true;
}

void Email::list_email(Node *n) {

  std::vector<Email> emails;
  static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  bool reload;

  size_t lines = 1;
  while (true) {

    emails.clear();
    if (!load_emails(n, &emails)) {
      return;
    }

    if (emails.size() == 0) {
      n->bprintf("|14You don't have any emails...|07\r\n");
      n->pause();
      return;
    }

    reload = false;

    n->cls();
    n->bprintf("|09 Msg#    Subject                          From             Date            |07\r\n");
    for (size_t i = 0; i < emails.size(); i++) {
      struct tm time_tm;
#ifdef _MSC_VER
      localtime_s(&time_tm, &emails.at(i).date);
#else
      localtime_r(&emails.at(i).date, &time_tm);
#endif

      if (emails.at(i).seen) {
        n->bprintf("|08[|15%6d|08] |14%-32.32s |13%-16.16s |11%02d:%02d %s %02d\r\n", i + 1, emails.at(i).subject.c_str(), emails.at(i).sender.c_str(),
                   time_tm.tm_hour, time_tm.tm_min, months[time_tm.tm_mon], time_tm.tm_mday);
      } else {
        n->bprintf("|08[|15%6d|08]|12*|14%-32.32s |13%-16.16s |11%02d:%02d %s %02d\r\n", i + 1, emails.at(i).subject.c_str(), emails.at(i).sender.c_str(),
                   time_tm.tm_hour, time_tm.tm_min, months[time_tm.tm_mon], time_tm.tm_mday);
      }

      lines++;
      if (lines == n->get_term_height() - 2) {
        n->bprintf("|14Select |08[|15%d|08-|15%d|08] |15Q|08=|14quit|08, |15ENTER|08=|14Continue |07", 1, emails.size());

        std::string res = n->get_str(6);

        if (res.size() == 0) {
          lines = 1;
          n->cls();
          n->bprintf("|09 Msg#    Subject                          From             Date            |07\r\n");
          continue;
        } else if (tolower(res[0]) == 'q') {
          return;
        } else {
          size_t emailno;
          try {
            emailno = (size_t)(std::stoi(res) - 1);
            while (emailno >= 0 && emailno < emails.size()) {
              int ret = view_email(n, emails.at(emailno), emailno, emails.size());

              emails.at(emailno).seen = true;

              if (ret == 0) {
                lines = 0;
                reload = true;
                break;
              } else {
                emailno += ret;
              }
            }
          } catch (std::invalid_argument const &) {
            return;
          } catch (std::out_of_range const &) {
            return;
          }
          if (reload) {
            break;
          }
        }
      }
    }

    if (reload) {
      continue;
    }

    n->bprintf("|14Select |08[|15%d|08-|15%d|08], |15ENTER|08=|14Quit |07", 1, emails.size());
    std::string res = n->get_str(6);

    if (res.size() == 0) {
      return;
    } else {
      size_t emailno;
      try {
        emailno = (size_t)(std::stoi(res) - 1);
        // view email
        while (emailno >= 0 && emailno < emails.size()) {
          int ret = view_email(n, emails.at(emailno), emailno, emails.size());
          emails.at(emailno).seen = true;
          if (ret == 0) {
            lines = 0;
            break;
          } else {
            emailno += ret;
          }
        }
      } catch (std::invalid_argument const &) {
        return;
      } catch (std::out_of_range const &) {
        return;
      }
    }
  }
}

int Email::view_email(Node *n, Email e, int emailno, int tot_emails) {
  struct tm time_tm;
  sqlite3 *db;
  sqlite3_stmt *stmt;
  static const char sql[] = "UPDATE email SET seen=1 WHERE id=?";

  if (!e.seen) {
    if (!open_database(n->get_data_path() + "/email.sqlite3", &db)) {
      n->log->log(LOG_ERROR, "Unable to open email sqlite database");
      return 0;
    }

    if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
      n->log->log(LOG_ERROR, "Unable to prepare email sqlite query");
      sqlite3_close(db);
      return 0;
    }

    sqlite3_bind_int(stmt, 1, e.id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
  }
#ifdef _MSC_VER
  localtime_s(&time_tm, &e.date);
#else
  localtime_r(&e.date, &time_tm);
#endif

  n->cls();
  n->bprintf("|10Subj: |15%s\r\n", e.subject.c_str());
  n->bprintf("|10From: |15%s\r\n", e.sender.c_str());

  struct tm date_tm;

  localtime_r(&e.date, &date_tm);

  n->bprintf("|10Date: |15%02d:%02d %02d-%02d-%04d\r\n", date_tm.tm_hour, date_tm.tm_min, date_tm.tm_mday, date_tm.tm_mon + 1, date_tm.tm_year + 1900);
  n->bprintf("|08-------------------------------------------------------------------------------|07\r\n");

  int lines = 4;

  for (size_t i = 0; i < e.msg.size(); i++) {
    n->bprintf("%s\r\n", e.msg.at(i).c_str());
    lines++;
    if (lines == 23) {
      n->pause();
      lines = 0;
    }
  }

  n->bprintf("|10(R)eply, (N)ext, (P)revious, (D)elete, (Q)uit: |07");
  char ch = n->getch();

  switch (tolower(ch)) {
  case 'n':
    return 1;
    break;
  case 'p':
    return -1;
    break;
  case 'd':
    delete_email(n, e.id);
    return 0;
  case 'r': {
    n->bprintf("\r\n");
    std::vector<std::string> qb;

    for (size_t q = 0; q < e.msg.size(); q++) {
      qb.push_back(" > " + e.msg.at(q));
    }

    enter_message(n, e.sender, e.subject, &qb);
  } break;
  case 'q':
    return 0;
  }

  return 0;
}

void Email::set_all_seen(Node *n) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  static const char sql[] = "UPDATE email SET seen=1 WHERE recipient = ?";

  if (!open_database(n->get_data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare email sqlite query");
    sqlite3_close(db);
    return;
  }

  std::string uname = n->get_username();

  sqlite3_bind_text(stmt, 1, uname.c_str(), -1, NULL);

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

void Email::delete_email(Node *n, int id) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  static const char sql[] = "DELETE FROM email WHERE id=?";

  if (!open_database(n->get_data_path() + "/email.sqlite3", &db)) {
    n->log->log(LOG_ERROR, "Unable to open email sqlite database");
    return;
  }

  if (sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL) != SQLITE_OK) {
    n->log->log(LOG_ERROR, "Unable to prepare email sqlite query");
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_int(stmt, 1, id);
  sqlite3_step(stmt);

  sqlite3_finalize(stmt);
  sqlite3_close(db);
}
