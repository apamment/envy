#pragma once

#include <sqlite3.h>
#include <string>
#include <vector>
class Node;

class Email {
public:
  std::string sender;
  std::string subject;
  std::vector<std::string> msg;
  bool seen = false;
  time_t date = 0;
  int id = 0;
  static int count_email(Node *n);
  static int unread_email(Node *n);
  static int view_email(Node *n, Email e, int emailno, int tot_emails);
  static void list_email(Node *n);
  static bool save_message(Node *n, std::string to, std::string from, std::string subject, std::vector<std::string> msg);
  static void delete_email(Node *n, int id);
  static void set_all_seen(Node *n);
  static void enter_message(Node *n, std::string recipient, std::string subject, std::vector<std::string> *quotebuffer);

private:
  static bool load_emails(Node *n, std::vector<Email> *emails);
  static bool open_database(std::string filename, sqlite3 **db);
};
