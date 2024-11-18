#include <sqlite3.h>
#include <string>
#include <fstream>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <openssl/evp.h>
#include "Node.h"
#include "User.h"

std::string User::hash_sha256(std::string pass, std::string salt) {
  std::stringstream ss;
  std::stringstream sh;
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int length_of_hash = 0;
  unsigned int i;

  ss.str("");
  ss << pass << salt;
  sh.str("");

  EVP_MD_CTX *context = EVP_MD_CTX_new();
  if (context != NULL) {
    if (EVP_DigestInit_ex(context, EVP_sha256(), NULL)) {
      if (EVP_DigestUpdate(context, ss.str().c_str(), strlen(ss.str().c_str()))) {
        if (EVP_DigestFinal_ex(context, hash, &length_of_hash)) {
          for (i = 0; i < length_of_hash; i++)
            sh << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << (int)hash[i];
          EVP_MD_CTX_free(context);
          return sh.str();
        }
      }
    }
    EVP_MD_CTX_free(context);
  } else {
    return "";
  }

  return "";
}


bool User::open_database(std::string path, sqlite3 **db) {
  static const char *create_users_sql =
      "CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, username TEXT COLLATE NOCASE UNIQUE, password TEXT, salt TEXT);";
  static const char *create_details_sql = "CREATE TABLE IF NOT EXISTS details(uid INTEGER, attrib TEXT COLLATE NOCASE, value TEXT COLLATE NOCASE);";

  int rc;
  char *err_msg = NULL;

  if (sqlite3_open(path.c_str(), db) != SQLITE_OK) {
    // std::cerr << "Unable to open database: users.db" << std::endl;
    return false;
  }
  sqlite3_busy_timeout(*db, 5000);

  rc = sqlite3_exec(*db, create_users_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    // std::cerr << "Unable to create user table: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }
  rc = sqlite3_exec(*db, create_details_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    // std::cerr << "Unable to create details table: " << err_msg << std::endl;
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }
  return true;
}

int User::check_password(Node *n, std::string username, std::string password) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int uid = -1;
    static const char *sql = "SELECT password, salt, id FROM users WHERE username = ?";

    if (open_database(n->get_data_path() + "/users.sqlite3", &db) == false) {
        return -1;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return -1;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, NULL);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string hash = hash_sha256(password, std::string((const char *)sqlite3_column_text(stmt, 1)));

        if (strcmp(hash.c_str(), (const char *)sqlite3_column_text(stmt, 0)) == 0) {
            uid = sqlite3_column_int(stmt, 2);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return uid;
}

std::string User::get_attrib(Node *n, std::string attrib, std::string fallback) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    std::string ret;
    static const char *sql = "SELECT value FROM details WHERE attrib = ? AND uid = ?";

    if (open_database(n->get_data_path() + "/users.sqlite3", &db) == false) {
        return fallback;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return fallback;
    }

    sqlite3_bind_text(stmt, 1, attrib.c_str(), -1, NULL);
    sqlite3_bind_int(stmt, 2, n->get_uid());

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ret = std::string((const char *)sqlite3_column_text(stmt, 0));
    } else {
        ret = fallback;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    
    return ret;
}

void User::set_attrib(Node *n, std::string attrib, std::string value) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    static const char *sql1 = "SELECT value FROM details WHERE attrib = ? AND uid = ?";
    static const char *sql2 = "UPDATE details SET value = ? WHERE attrib = ? and uid = ?";
    static const char *sql3 = "INSERT INTO details (uid, attrib, value) VALUES(?, ?, ?)";
    if (open_database(n->get_data_path() + "/users.sqlite3", &db) == false) {
        return;
    }

    if (sqlite3_prepare_v2(db, sql1, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return;
    }

    sqlite3_bind_text(stmt, 1, attrib.c_str(), -1, NULL);
    sqlite3_bind_int(stmt, 2, n->get_uid());

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        if (sqlite3_prepare_v2(db, sql2, -1, &stmt, NULL) != SQLITE_OK) {
            sqlite3_close(db);
            return;
        }
        sqlite3_bind_text(stmt, 1, value.c_str(), -1, NULL);
        sqlite3_bind_text(stmt, 2, attrib.c_str(), -1, NULL);
        sqlite3_bind_int(stmt, 3, n->get_uid());

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    } else {
        sqlite3_finalize(stmt);
        if (sqlite3_prepare_v2(db, sql3, -1, &stmt, NULL) != SQLITE_OK) {
            sqlite3_close(db);
            return;
        }
        sqlite3_bind_int(stmt, 1, n->get_uid());
        sqlite3_bind_text(stmt, 2, attrib.c_str(), -1, NULL);
        sqlite3_bind_text(stmt, 3, value.c_str(), -1, NULL);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }
}

int User::inst_user(Node *n, std::string username, std::string password) {
    sqlite3 *db;
    sqlite3_stmt *stmt;

    unsigned char salt[11];
    std::string hash;
    std::stringstream ssalt;
    memset(salt, 0, 11);

    FILE *fptr = fopen("/dev/urandom", "r");
    if (!fptr) {
        return -1;
    }

    fread(salt, 1, 10, fptr);

    fclose(fptr);

    for (int i = 0; i < 10; i++) {
        ssalt << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << (int)salt[i];
    }


    std::string saltstr = ssalt.str();

    hash = hash_sha256(password, saltstr);
    if (hash.size() == 0) {
        return -1;
    }



    static const char *sql = "INSERT INTO users (username, password, salt) VALUES(?, ?, ?)";



    if (open_database(n->get_data_path() + "/users.sqlite3", &db) == false) {
        return -1;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return -1;
    }


    sqlite3_bind_text(stmt, 1, username.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 2, hash.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 3, saltstr.c_str(), -1, NULL);

    sqlite3_step(stmt);
    int id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return id;
}

bool User::valid_fullname(Node *n, std::string fullname) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    static const char *sql = "SELECT uid FROM details WHERE attrib = 'fullname' AND value = ?";

    if (open_database(n->get_data_path() + "/users.sqlite3", &db) == false) {
        return false;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return false;
    }

    sqlite3_bind_text(stmt, 1, fullname.c_str(), -1, NULL);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return true;
}

User::InvalidUserReason User::valid_username(Node *n, std::string username) {

    if (username.length() < 2) {
        return User::InvalidUserReason::TOOSHORT;
    }

    sqlite3 *db;
    sqlite3_stmt *stmt;
    static const char *sql = "SELECT username FROM users WHERE username = ?";

    if (open_database(n->get_data_path() + "/users.sqlite3", &db) == false) {
        return User::InvalidUserReason::ERROR;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        return User::InvalidUserReason::ERROR;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, NULL);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return User::InvalidUserReason::INUSE;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    std::ifstream inf(n->get_data_path() + "/trashcan.txt");

    if (inf.is_open()) {
        std::string line;
        while(getline(inf, line)) {
            if (strcasecmp(line.c_str(), username.c_str()) == 0) {
                inf.close();
                return User::InvalidUserReason::TRASHCAN;
            }
        }

        inf.close();
    }
    
    return User::InvalidUserReason::OK;
}