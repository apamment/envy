#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include "MessageBase.h"
#include "FileBase.h"
#include "Node.h"

std::string FileBase::strip_ansi(std::string msg) {
  std::stringstream ss;
  std::vector<int> param;
  bool got_esc = false;
  bool got_brac = false;
  for (size_t i = 0;i < msg.length(); i++) {
    if (msg.at(i) == 0x1b) {
      param.clear();
      param.push_back(0);
      got_esc = true;
    } else {
      if (got_esc) {
        if (msg.at(i) == '[') {
          got_esc = false;
          got_brac = true;
        }
      } else if (got_brac) {
        if (msg.at(i) >= '0' && msg.at(i) <= '9') {
          param.at(param.size() - 1) += msg.at(i) - '0';
        } else if (msg.at(i) == ';') {
          param.push_back(0);
        } else if (msg.at(i) == 'C') {
          for (int j = 0; j < param.at(0); j++) {
            ss << ' '; 
          }
          got_brac = false;
        } else {
          got_brac = false;
        }
      } else {
        ss << msg.at(i);
      }
    }
  }

  return ss.str();
}

void FileBase::list_files(Node *n) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  std::vector<struct file_s> files;

  static const char *sql = "SELECT filename, description, uploaddate, uploadedby downloadcount FROM files";

  if (!open_database(n->get_data_path() + "/" + database + ".sqlite3", &db)) {
    n->bprintf("|12Failed to open file base|07\r\n");
    n->pause();
    return;
  }
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    n->bprintf("|12Failed to read file base|07\r\n");
    n->pause();
    return;
  }
  while(sqlite3_step(stmt) == SQLITE_ROW) {
    struct file_s f;
    f.fb = this;
    f.filename = std::filesystem::path((const char *)sqlite3_column_text(stmt, 0));
    f.uploadedby = std::string((const char *)sqlite3_column_text(stmt, 3));
    f.uploaddate = sqlite3_column_int64(stmt, 2);
    f.downloadcount = sqlite3_column_int(stmt, 4);
    f.size = std::filesystem::exists(f.filename) ? std::filesystem::file_size(f.filename) : -1;
    std::string tmp = strip_ansi(std::string((const char*)sqlite3_column_text(stmt, 1)));


    std::stringstream ss;

    if (tmp.size() == 0) {
        f.desc.push_back("No Description.");
    } else {
      for (size_t i = 0; i < tmp.size(); i++) {
        if (tmp.at(i) == '\n') {
          f.desc.push_back(ss.str());
          ss.str("");
        } else if (tmp.at(i) == 0x1a) {
          break;
        } else if (tmp.at(i) != '\r') {
          ss << tmp.at(i);
        }
      }

      if (ss.str() != "") {
          f.desc.push_back(ss.str());
      }
    }
    f.desc.push_back("  |15-> |10Uploaded By: |14" + f.uploadedby);
    files.push_back(f);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  if (files.size() == 0) { 
    n->bprintf("|12No files in file base...|07\r\n");
    n->pause();
    return;
  }

  int lines = 0;

  for (size_t i = 0; i < files.size(); i++) {
    if (files.at(i).size > 0) {
      int64_t sz = 0;
      char unit;
      if (files.at(i).size < 1024) {
        sz = files.at(i).size;
        unit = 'b';
      } else if (files.at(i).size < 1024 * 1024) {
        sz = files.at(i).size / 1024;
        unit = 'K';
      } else if (files.at(i).size < 1024 * 1024 * 1024) {
        sz = files.at(i).size / 1024 / 1024;
        unit = 'M';
      } else {
        sz = files.at(i).size / 1024 / 1024 / 1024;
        unit = 'G';
      }
      n->bprintf("|15%3d|07. |14%-20.20s |10%4d%c |07%-46.46s\r\n", i + 1, files.at(i).filename.filename().u8string().c_str(), sz, unit, files.at(i).desc.at(0).c_str()); 
    } else {
      n->bprintf("|15%3d|07. |14%-20.20s  |12MSNG |07%-46.46s\r\n", i + 1, files.at(i).filename.filename().u8string().c_str(), files.at(i).desc.at(0).c_str()); 
    }
    lines++;
    if (lines == n->get_term_height() - 1) {
      n->bprintf("|13MORE |15- |10(Q) Quit or Tag File: ");
      std::string num = n->get_str(4);

      if (num.size() > 0) {
        if (tolower(num.at(0)) == 'q') {
          return;
        }

        try {
          int no = std::stoi(num);

          if (no - 1 >= 0 && no - 1 < files.size()) {
            n->tag_file(files.at(no-1));
          }
        } catch (std::out_of_range const &) {
        } catch (std::invalid_argument const &) {
        }
      }
      lines = 0;
    }
    for (size_t j = 1; j < files.at(i).desc.size(); j++) {
      n->bprintf("                                |07%-46.46s\r\n",files.at(i).desc.at(j).c_str());
      lines++;
      if (lines == n->get_term_height() - 1) {
        n->bprintf("|13MORE |15- |10(Q) Quit or Tag File: ");
        std::string num = n->get_str(4);

        if (num.size() > 0) {
          if (tolower(num.at(0)) == 'q') {
            return;
          }

          try {
            int no = std::stoi(num);

            if (no - 1 >= 0 && no - 1 < files.size()) {
              n->tag_file(files.at(no-1));
            }
          } catch (std::out_of_range const &) {
          } catch (std::invalid_argument const &) {
          }
        }
        lines = 0;
      } 
    }
  }
  n->bprintf("|11END  |15- |10Tag File: ");
  std::string num = n->get_str(4);
  try {
    int no = std::stoi(num);

    if (no - 1 >= 0 && no - 1 < files.size()) {
      n->tag_file(files.at(no-1));
    }
  } catch (std::out_of_range const &) {
  } catch (std::invalid_argument const &) {
  }
}

void FileBase::inc_download(Node *n, std::string filename) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  static const char * sql = "UPDATE files SET downloadcount = downloadcount + 1 WHERE filename = ?";

  if (!open_database(n->get_data_path() + "/" + database + ".sqlite3", &db)) {
    return;
  }

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, NULL);

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);
}

bool FileBase::open_database(std::string path, sqlite3 **db) {
  static const char *create_files_sql =
      "CREATE TABLE IF NOT EXISTS files(id INTEGER PRIMARY KEY, filename TEXT, description TEXT, uploaddate INTEGER, uploadedby TEXT, downloadcount INTEGER);";

  int rc;
  char *err_msg = NULL;

  if (sqlite3_open(path.c_str(), db) != SQLITE_OK) {
    // std::cerr << "Unable to open database: users.db" << std::endl;
    return false;
  }
  sqlite3_busy_timeout(*db, 5000);

  rc = sqlite3_exec(*db, create_files_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    // std::cerr << "Unable to create user table: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }
  return true;
}

bool FileBase::insert_file(Node *n, std::string filename, std::vector<std::string> description) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  std::stringstream ss;


  for (size_t i = 0; i < description.size(); i++) {
    ss << description.at(i) << "\n";
  }

  std::string d = ss.str();
  time_t now = time(NULL);

  static const char * sql = "INSERT INTO files (filename, description, uploaddate, uploadedby, downloadcount) VALUES(?, ?, ?, ?, 0)";

  if (!open_database(n->get_data_path() + "/" + database + ".sqlite3", &db)) {
    return false;
  }

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return false;
  }

  sqlite3_bind_text(stmt, 1, filename.c_str(), -1, NULL);
  sqlite3_bind_text(stmt, 2, d.c_str(), -1, NULL);
  sqlite3_bind_int64(stmt, 3, now);
  sqlite3_bind_text(stmt, 4, n->get_username().c_str(), -1, NULL);

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return true;
}

int FileBase::count(Node *n) {
  sqlite3 *db;
  sqlite3_stmt *stmt;

  static const char * sql = "SELECT COUNT(*) FROM files";

  if (!open_database(n->get_data_path() + "/" + database + ".sqlite3", &db)) {
    return 0;
  }

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return 0;
  }

  int ret = 0;

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ret = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return ret;
}