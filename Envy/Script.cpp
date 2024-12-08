#include <fstream>
#include <sstream>
#include <sqlite3.h>
#include <sys/utsname.h>
#include "Script.h"
#include "Node.h"
#include "User.h"
#include "duktape.h"
#include "MessageBase.h"
#include "Email.h"
#include "Version.h"
#include "FullScreenEditor.h"

struct script_data {
  Node *n;
  std::string script;
};

bool Script::open_database(Node *n, sqlite3 **db) {
  static const char *create_details_sql =
      "CREATE TABLE IF NOT EXISTS details(script TEXT COLLATE NOCASE, attrib TEXT COLLATE NOCASE, value TEXT COLLATE NOCASE);";

  int rc;
  char *err_msg = NULL;

  if (sqlite3_open(std::string(n->get_data_path() + "/script_data.sqlite3").c_str(), db) != SQLITE_OK) {
    return false;
  }
  sqlite3_busy_timeout(*db, 5000);

  rc = sqlite3_exec(*db, create_details_sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK) {
    sqlite3_free(err_msg);
    sqlite3_close(*db);
    return false;
  }
  return true;
}

std::string Script::get_attrib(Node *n, std::string script, std::string attrib, std::string fallback) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  std::string ret;
  static const char *sql = "SELECT value FROM details WHERE attrib = ? AND script = ?";

  if (open_database(n, &db) == false) {
    return fallback;
  }

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return fallback;
  }

  sqlite3_bind_text(stmt, 1, attrib.c_str(), -1, NULL);
  sqlite3_bind_text(stmt, 2, script.c_str(), -1, NULL);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    ret = std::string((const char *)sqlite3_column_text(stmt, 0));
  } else {
    ret = fallback;
  }
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return ret;
}

void Script::set_attrib(Node *n, std::string script, std::string attrib, std::string value) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  static const char *sql1 = "SELECT value FROM details WHERE attrib = ? AND script = ?";
  static const char *sql2 = "UPDATE details SET value = ? WHERE attrib = ? and script = ?";
  static const char *sql3 = "INSERT INTO details (script, attrib, value) VALUES(?, ?, ?)";
  if (open_database(n, &db) == false) {
    return;
  }

  if (sqlite3_prepare_v2(db, sql1, -1, &stmt, NULL) != SQLITE_OK) {
    sqlite3_close(db);
    return;
  }

  sqlite3_bind_text(stmt, 1, attrib.c_str(), -1, NULL);
  sqlite3_bind_text(stmt, 2, script.c_str(), -1, NULL);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    sqlite3_finalize(stmt);
    if (sqlite3_prepare_v2(db, sql2, -1, &stmt, NULL) != SQLITE_OK) {
      sqlite3_close(db);
      return;
    }
    sqlite3_bind_text(stmt, 1, value.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 2, attrib.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 3, script.c_str(), -1, NULL);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
  } else {
    sqlite3_finalize(stmt);
    if (sqlite3_prepare_v2(db, sql3, -1, &stmt, NULL) != SQLITE_OK) {
      sqlite3_close(db);
      return;
    }
    sqlite3_bind_text(stmt, 1, script.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 2, attrib.c_str(), -1, NULL);
    sqlite3_bind_text(stmt, 3, value.c_str(), -1, NULL);

    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
  }
}

static void my_fatal(void *udata, const char *msg) {
  struct script_data *sd = (struct script_data *)udata;
  Node *n = (Node *)sd->n;

  n->disconnect();
}

static Node *get_node(duk_context *ctx) {
  duk_memory_functions funcs;
  duk_get_memory_functions(ctx, &funcs);

  struct script_data *sd = (struct script_data *)funcs.udata;

  return sd->n;
}

static std::string get_script(duk_context *ctx) {
  duk_memory_functions funcs;
  duk_get_memory_functions(ctx, &funcs);

  struct script_data *sd = (struct script_data *)funcs.udata;

  return sd->script;
}

static duk_ret_t bdisconnect(duk_context *ctx) {
  Node *n = get_node(ctx);
  n->disconnect();
  return 0;
}

static duk_ret_t bprint(duk_context *ctx) {
  Node *n = get_node(ctx);
  n->bprintf("%s", duk_to_string(ctx, 0));
  return 0;
}

static duk_ret_t bgetch(duk_context *ctx) {
  Node *n = get_node(ctx);
  char c = n->getch();

  duk_push_lstring(ctx, &c, 1);

  return 1;
}

static duk_ret_t bgetstr(duk_context *ctx) {
  Node *n = get_node(ctx);
  int len = duk_to_number(ctx, 0);
  duk_push_string(ctx, n->get_str(len).c_str());

  return 1;
}

static duk_ret_t bgetpass(duk_context *ctx) {
  Node *n = get_node(ctx);
  int len = duk_to_number(ctx, 0);
  duk_push_string(ctx, n->get_str(len, '*').c_str());

  return 1;
}

static duk_ret_t bgetattr(duk_context *ctx) {
  Node *n = get_node(ctx);
  std::string attr = std::string(duk_to_string(ctx, 0));
  std::string def = std::string(duk_to_string(ctx, -1));

  duk_push_string(ctx, User::get_attrib(n, attr, def).c_str());
  return 1;
}

static duk_ret_t bputattr(duk_context *ctx) {
  Node *n = get_node(ctx);
  std::string attr = std::string(duk_to_string(ctx, 0));
  std::string val = std::string(duk_to_string(ctx, -1));

  User::set_attrib(n, attr, val);

  return 0;
}

static duk_ret_t bgetattro(duk_context *ctx) {
  Node *n = get_node(ctx);
  uint32_t uid = duk_to_uint32(ctx, 0);
  std::string attr = std::string(duk_to_string(ctx, -2));
  std::string def = std::string(duk_to_string(ctx, -1));

  // printf("%u %s %s\n", uid, attr.c_str(), def.c_str());

  duk_push_string(ctx, User::get_attrib(n, uid, attr, def).c_str());
  return 1;
}

static duk_ret_t bputattro(duk_context *ctx) {
  Node *n = get_node(ctx);
  uint32_t uid = duk_to_uint32(ctx, 0);
  std::string attr = std::string(duk_to_string(ctx, -2));
  std::string val = std::string(duk_to_string(ctx, -1));

  User::set_attrib(n, uid, attr, val);

  return 0;
}

static duk_ret_t bcls(duk_context *ctx) {
  Node *n = get_node(ctx);
  n->cls();
  return 0;
}

static duk_ret_t bputgfile(duk_context *ctx) {
  Node *n = get_node(ctx);
  std::string gfile = std::string(duk_to_string(ctx, 0));
  bool success = n->putgfile(gfile);
  duk_push_boolean(ctx, success);
  return 1;
}

static duk_ret_t bexec(duk_context *ctx) {
  Node *n = get_node(ctx);
  std::string script = std::string(duk_to_string(ctx, 0));
  int ret = Script::run(n, script);

  duk_push_number(ctx, ret);
  return 1;
}

static duk_ret_t bsaveglobal(duk_context *ctx) {
  Node *n = get_node(ctx);

  Script::set_attrib(n, "GLOBAL", std::string(duk_to_string(ctx, 0)), std::string(duk_to_string(ctx, -1)));

  return 0;
}

static duk_ret_t bsaveval(duk_context *ctx) {
  Node *n = get_node(ctx);
  std::string script = get_script(ctx);

  Script::set_attrib(n, script, std::string(duk_to_string(ctx, 0)), std::string(duk_to_string(ctx, -1)));

  return 0;
}

static duk_ret_t bloadglobal(duk_context *ctx) {
  Node *n = get_node(ctx);

  duk_push_string(ctx, Script::get_attrib(n, "GLOBAL", std::string(duk_to_string(ctx, 0)), std::string(duk_to_string(ctx, -1))).c_str());

  return 1;
}

static duk_ret_t bloadval(duk_context *ctx) {
  Node *n = get_node(ctx);
  std::string script = get_script(ctx);

  duk_push_string(ctx, Script::get_attrib(n, script, std::string(duk_to_string(ctx, 0)), std::string(duk_to_string(ctx, -1))).c_str());

  return 1;
}

static duk_ret_t bgetusername(duk_context *ctx) {
  Node *n = get_node(ctx);
  duk_push_string(ctx, n->get_username().c_str());
  return 1;
}

static duk_ret_t bgetusernameo(duk_context *ctx) {
  Node *n = get_node(ctx);
  duk_push_string(ctx, User::getusername(n, duk_get_int(ctx, 0)).c_str());

  return 1;
}

static duk_ret_t blistfiles(duk_context *ctx) {
  Node *n = get_node(ctx);

  FileBase *fb = n->get_curr_filebase();
  if (fb != nullptr) {
    fb->list_files(n);
  }
  return 0;
}

static duk_ret_t blistmsgs(duk_context *ctx) {
  Node *n = get_node(ctx);

  int startingat = duk_get_int(ctx, 0);

  MessageBase *mb = n->get_curr_msgbase();
  if (mb != nullptr) {
    mb->list_messages(n, startingat);
  }
  return 0;
}

static duk_ret_t breadmsgs(duk_context *ctx) {
  Node *n = get_node(ctx);

  int startingat = duk_get_int(ctx, 0);

  MessageBase *mb = n->get_curr_msgbase();
  if (mb != nullptr) {
    mb->read_messages(n, startingat);
  }
  return 0;
}

static duk_ret_t bwritemsg(duk_context *ctx) {
  Node *n = get_node(ctx);

  MessageBase *mb = n->get_curr_msgbase();
  if (mb != nullptr) {
    mb->enter_message(n, "", "", nullptr, nullptr);
  }
  return 0;
}

static duk_ret_t bselectgroup(duk_context *ctx) {
  Node *n = get_node(ctx);

  n->select_msg_group();

  return 0;
}

static duk_ret_t bselectarea(duk_context *ctx) {
  Node *n = get_node(ctx);

  n->select_msg_base();

  return 0;
}

static duk_ret_t bselectfilearea(duk_context *ctx) {
  Node *n = get_node(ctx);

  n->select_file_base();

  return 0;
}

static duk_ret_t bgetarea(duk_context *ctx) {
  Node *n = get_node(ctx);

  if (n->get_curr_msgbase() != nullptr) {
    duk_push_string(ctx, n->get_curr_msgbase()->name.c_str());
  } else {
    duk_push_string(ctx, "Nothing");
  }

  return 1;
}

static duk_ret_t bgetfilearea(duk_context *ctx) {
  Node *n = get_node(ctx);

  if (n->get_curr_filebase() != nullptr) {
    duk_push_string(ctx, n->get_curr_filebase()->name.c_str());
  } else {
    duk_push_string(ctx, "Nothing");
  }

  return 1;
}

static duk_ret_t bpause(duk_context *ctx) {
  Node *n = get_node(ctx);

  n->pause();

  return 0;
}

static duk_ret_t bscanbases(duk_context *ctx) {
  Node *n = get_node(ctx);

  n->scan_msg_bases();

  return 0;
}

static duk_ret_t brundoor(duk_context *ctx) {
  Node *n = get_node(ctx);

  std::string doorkey = duk_get_string(ctx, 0);

  n->launch_door(doorkey);

  return 0;
}

static duk_ret_t bgetmsglr(duk_context *ctx) {
  Node *n = get_node(ctx);

  duk_push_number(ctx, n->get_curr_msgbase()->get_highread(n));

  return 1;
}

static duk_ret_t bgetmsgtot(duk_context *ctx) {
  Node *n = get_node(ctx);

  duk_push_number(ctx, n->get_curr_msgbase()->get_total(n));

  return 1;
}

static duk_ret_t blistemail(duk_context *ctx) {
  Node *n = get_node(ctx);

  Email::list_email(n);

  return 0;
}

static duk_ret_t blisttaggedfiles(duk_context *ctx) {
  Node *n = get_node(ctx);

  n->list_tagged_files();

  return 0;
}

static duk_ret_t benteremail(duk_context *ctx) {
  Node *n = get_node(ctx);

  Email::enter_message(n, std::string(duk_get_string(ctx, 0)), std::string(duk_get_string(ctx, -1)), nullptr);
  return 0;
}

static duk_ret_t bcheckuser(duk_context *ctx) {
  Node *n = get_node(ctx);

  duk_push_string(ctx, User::exists(n, std::string(duk_get_string(ctx, 0))).c_str());
  return 1;
}

static duk_ret_t bcountemail(duk_context *ctx) {
  Node *n = get_node(ctx);
  duk_push_number(ctx, Email::count_email(n));

  return 1;
}

static duk_ret_t bunreademail(duk_context *ctx) {
  Node *n = get_node(ctx);
  duk_push_number(ctx, Email::unread_email(n));

  return 1;
}

static duk_ret_t bgettimeleft(duk_context *ctx) {
  Node *n = get_node(ctx);
  duk_push_number(ctx, n->get_timeleft());
  return 1;
}

static duk_ret_t bsetaction(duk_context *ctx) {
  Node *n = get_node(ctx);
  n->action(std::string(duk_get_string(ctx, 0)));

  return 0;
}

static duk_ret_t bgetusers(duk_context *ctx) {
  Node *n = get_node(ctx);
  std::vector<struct userid_s> users = User::get_users(n);

  duk_idx_t arr_idx;
  arr_idx = duk_push_array(ctx);
  for (size_t i = 0; i < users.size(); i++) {
    duk_idx_t obj_idx = duk_push_object(ctx);
    duk_push_number(ctx, users.at(i).uid);
    duk_put_prop_string(ctx, obj_idx, "uid");
    duk_push_string(ctx, users.at(i).username.c_str());
    duk_put_prop_string(ctx, obj_idx, "username");
    duk_put_prop_index(ctx, arr_idx, i);
  }
  return 1;
}

static duk_ret_t bdownload(duk_context *ctx) {
  Node *n = get_node(ctx);

  n->download_tagged_files();

  return 0;
}

static duk_ret_t bcleartagged(duk_context *ctx) {
  Node *n = get_node(ctx);
  
  n->clear_tagged_files();

  return 0;
}

static duk_ret_t bupload(duk_context *ctx) {
  Node *n = get_node(ctx);
  
  n->upload();

  return 0;
}

static duk_ret_t bgetopname(duk_context *ctx) {
  Node *n = get_node(ctx);

  duk_push_string(ctx, n->get_opname().c_str());

  return 1;
}

static duk_ret_t bgetversion(duk_context *ctx) {
  duk_idx_t obj_idx = duk_push_object(ctx);
  duk_push_string(ctx, std::string(VERSION "-" GITV).c_str());
  duk_put_prop_string(ctx, obj_idx, "envy");

  struct utsname u;

  if (uname(&u) == 0) {
    duk_push_string(ctx, u.sysname);
    duk_put_prop_string(ctx, obj_idx, "system");
    duk_push_string(ctx, u.release);
    duk_put_prop_string(ctx, obj_idx, "sysver");
    duk_push_string(ctx, u.machine);
    duk_put_prop_string(ctx, obj_idx, "machine");

  } else {
    duk_push_string(ctx, "???");
    duk_put_prop_string(ctx, obj_idx, "system");
    duk_push_string(ctx, "???");
    duk_put_prop_string(ctx, obj_idx, "sysver");
    duk_push_string(ctx, "???");
    duk_put_prop_string(ctx, obj_idx, "machine");
  }

  std::stringstream ss;
  ss << (DUK_VERSION / 10000) << "." << (DUK_VERSION % 10000 / 100) << "." << (DUK_VERSION % 100);
  duk_push_string(ctx, ss.str().c_str());
  duk_put_prop_string(ctx, obj_idx, "duktape");

  return 1;
}

static duk_ret_t bgetdoors(duk_context *ctx) {
  Node *n = get_node(ctx);
  std::vector<struct door_cfg_s> doors = n->get_doors();

  duk_idx_t arr_idx;
  arr_idx = duk_push_array(ctx);
  for (size_t i = 0; i < doors.size(); i++) {
    duk_idx_t obj_idx = duk_push_object(ctx);
    duk_push_string(ctx, doors.at(i).key.c_str());
    duk_put_prop_string(ctx, obj_idx, "key");
    duk_push_string(ctx, doors.at(i).name.c_str());
    duk_put_prop_string(ctx, obj_idx, "name");
    duk_put_prop_index(ctx, arr_idx, i);
  }
  return 1;
}

static duk_ret_t bgetactions(duk_context *ctx) {
  Node *n = get_node(ctx);
  std::vector<struct nodeuse_s> nodeuse = n->get_actions();

  duk_idx_t arr_idx;
  arr_idx = duk_push_array(ctx);
  for (size_t i = 0; i < nodeuse.size(); i++) {
    duk_idx_t obj_idx = duk_push_object(ctx);
    duk_push_number(ctx, nodeuse.at(i).uid);
    duk_put_prop_string(ctx, obj_idx, "uid");
    duk_push_string(ctx, nodeuse.at(i).action.c_str());
    duk_put_prop_string(ctx, obj_idx, "action");
    duk_put_prop_index(ctx, arr_idx, i);
  }
  return 1;
}

static duk_ret_t bsetpassword(duk_context *ctx) {
  Node *n = get_node(ctx);

  std::string newpass = std::string(duk_get_string(ctx, 0));

  User::setpassword(n, n->get_uid(), newpass);

  return 0;
}

static duk_ret_t bsetpasswordo(duk_context *ctx) {
  Node *n = get_node(ctx);

  int uid = duk_get_int(ctx, 0);
  std::string newpass = std::string(duk_get_string(ctx, -1));

  User::setpassword(n, uid, newpass);

  return 0;
}

static duk_ret_t bcheckpass(duk_context *ctx) {
  Node *n = get_node(ctx);

  std::string pass = std::string(duk_get_string(ctx, 0));

  if (User::check_password(n, n->get_username(), pass) > 0) {
    duk_push_boolean(ctx, true);
  } else {
    duk_push_boolean(ctx, false);
  }
  return 1;
}

static duk_ret_t bisvisible(duk_context *ctx) {
  Node *n = get_node(ctx);

  duk_push_boolean(ctx, n->get_visible());

  return 1;
}

static duk_ret_t bsendfile(duk_context *ctx) {
  Node *n = get_node(ctx);

  n->send_file(std::string(duk_get_string(ctx, 0)));

  return 0;
}

static duk_ret_t beditsig(duk_context *ctx) {
  Node *n = get_node(ctx);

  std::vector<std::string> oldsig;
  std::vector<std::string> newsig;

  std::string sig = User::get_attrib(n, "signature", "");

  std::stringstream ss(sig);
  std::string line;

  while(getline(ss, line, '\r')) {
    oldsig.push_back(line);
  }

  FullScreenEditor fse(n, "Signature Editor", "Signature Editor", nullptr, &oldsig);

  newsig = fse.edit();

  std::stringstream ss2;

  if (newsig.size() > 0) {
    for (size_t i = 0; i < newsig.size(); i++) {
      ss2 << newsig.at(i) << '\r';
    }

    User::set_attrib(n, "signature", ss2.str());
  }
  return 0;
}

static duk_ret_t breadmsg(duk_context *ctx) {
  Node *n = get_node(ctx);
  MessageBase *mb = n->get_msgbase(std::string(duk_get_string(ctx, 0)));

  if (mb != nullptr) {
    struct msg_s *m = mb->get_message(n, duk_get_int(ctx, -1));
    if (m != nullptr) {
      duk_idx_t obj_idx = duk_push_object(ctx);
      duk_push_number(ctx, m->id);
      duk_put_prop_string(ctx, obj_idx, "idx");
      duk_push_string(ctx, m->from.c_str());
      duk_put_prop_string(ctx, obj_idx, "from");
      duk_push_string(ctx, m->to.c_str());
      duk_put_prop_string(ctx, obj_idx, "to");
      duk_push_string(ctx, m->subject.c_str());
      duk_put_prop_string(ctx, obj_idx, "subject");
      duk_push_string(ctx, m->body.c_str());
      duk_put_prop_string(ctx, obj_idx, "body");

      delete m;
      return 1;
    }
  }

  duk_push_undefined(ctx);
  return 1;
}

// postmsg(file, to, from, subject, body)
static duk_ret_t bpostmsg(duk_context *ctx) {
  Node *n = get_node(ctx);
  MessageBase *mb = n->get_msgbase(std::string(duk_get_string(ctx, 0)));

  std::string body = std::string(duk_get_string(ctx, -1));
  std::string subject = std::string(duk_get_string(ctx, -2));
  std::string from = std::string(duk_get_string(ctx, -3));
  std::string to = std::string(duk_get_string(ctx, -4));

  std::stringstream ss;
  std::vector<std::string> msg;

  for (size_t i = 0; i < body.size(); i++) {
    if (body.at(i) == '\n' && i > 1 && body.at(i - 1) != '\r') {
      msg.push_back(ss.str());
      ss.str("");
    } else if (body.at(i) == '\r') {
      msg.push_back(ss.str());
      ss.str("");
    } else if (body.at(i) != '\n') {
      ss << body.at(i);
    }
  }

  if (ss.str().size() > 0) {
    msg.push_back(ss.str());
  }

  duk_push_boolean(ctx, mb->save_message(n, to, from, subject, msg, std::string(""), std::string("")));

  return 1;
}

int Script::run(Node *n, std::string script) {
  std::string filename = n->get_script_path() + "/" + script + ".js";
  std::ifstream t(filename);
  std::stringstream buffer;
  struct script_data sd;

  sd.n = n;
  sd.script = script;

  n->log->log(LOG_INFO, "Loading \"%s\"", filename.c_str());

  if (t.is_open()) {
    buffer << t.rdbuf();
  } else {
    n->log->log(LOG_ERROR, "Script \"%s\" is missing!", filename.c_str());
    return -1;
  }

  duk_context *ctx = duk_create_heap(NULL, NULL, NULL, (void *)&sd, my_fatal);

  duk_push_c_function(ctx, bprint, 1);
  duk_put_global_string(ctx, "print");

  duk_push_c_function(ctx, bgetch, 0);
  duk_put_global_string(ctx, "getch");

  duk_push_c_function(ctx, bdisconnect, 0);
  duk_put_global_string(ctx, "disconnect");

  duk_push_c_function(ctx, bgetstr, 1);
  duk_put_global_string(ctx, "gets");

  duk_push_c_function(ctx, bgetpass, 1);
  duk_put_global_string(ctx, "getpass");

  duk_push_c_function(ctx, bgetattr, 2);
  duk_put_global_string(ctx, "getattr");

  duk_push_c_function(ctx, bputattr, 2);
  duk_put_global_string(ctx, "putattr");

  duk_push_c_function(ctx, bgetattro, 3);
  duk_put_global_string(ctx, "getattro");

  duk_push_c_function(ctx, bputattro, 3);
  duk_put_global_string(ctx, "putattro");

  duk_push_c_function(ctx, bcls, 0);
  duk_put_global_string(ctx, "cls");

  duk_push_c_function(ctx, bputgfile, 1);
  duk_put_global_string(ctx, "gfile");

  duk_push_c_function(ctx, bexec, 1);
  duk_put_global_string(ctx, "exec");

  duk_push_c_function(ctx, bsaveval, 2);
  duk_put_global_string(ctx, "save");

  duk_push_c_function(ctx, bloadval, 2);
  duk_put_global_string(ctx, "load");

  duk_push_c_function(ctx, bsaveglobal, 2);
  duk_put_global_string(ctx, "globalsave");

  duk_push_c_function(ctx, bloadglobal, 2);
  duk_put_global_string(ctx, "globalload");

  duk_push_c_function(ctx, bgetusername, 0);
  duk_put_global_string(ctx, "getusername");

  duk_push_c_function(ctx, blistmsgs, 1);
  duk_put_global_string(ctx, "listmsgs");

  duk_push_c_function(ctx, breadmsgs, 1);
  duk_put_global_string(ctx, "readmsgs");

  duk_push_c_function(ctx, bwritemsg, 0);
  duk_put_global_string(ctx, "writemsg");

  duk_push_c_function(ctx, bselectgroup, 0);
  duk_put_global_string(ctx, "selectgroup");

  duk_push_c_function(ctx, bselectarea, 0);
  duk_put_global_string(ctx, "selectarea");

  duk_push_c_function(ctx, bgetarea, 0);
  duk_put_global_string(ctx, "getarea");

  duk_push_c_function(ctx, bpause, 0);
  duk_put_global_string(ctx, "pause");

  duk_push_c_function(ctx, bscanbases, 0);
  duk_put_global_string(ctx, "scanbases");

  duk_push_c_function(ctx, brundoor, 1);
  duk_put_global_string(ctx, "rundoor");

  duk_push_c_function(ctx, bgetdoors, 0);
  duk_put_global_string(ctx, "getdoors");

  duk_push_c_function(ctx, bgetmsglr, 0);
  duk_put_global_string(ctx, "getmsglr");

  duk_push_c_function(ctx, bgetmsgtot, 0);
  duk_put_global_string(ctx, "getmsgtot");

  duk_push_c_function(ctx, blistemail, 0);
  duk_put_global_string(ctx, "listemail");

  duk_push_c_function(ctx, benteremail, 2);
  duk_put_global_string(ctx, "enteremail");

  duk_push_c_function(ctx, bcheckuser, 1);
  duk_put_global_string(ctx, "checkuser");

  duk_push_c_function(ctx, bcountemail, 0);
  duk_put_global_string(ctx, "countemail");

  duk_push_c_function(ctx, bunreademail, 0);
  duk_put_global_string(ctx, "unreademail");

  duk_push_c_function(ctx, bgettimeleft, 0);
  duk_put_global_string(ctx, "timeleft");

  duk_push_c_function(ctx, bgetusers, 0);
  duk_put_global_string(ctx, "getusers");

  duk_push_c_function(ctx, bgetopname, 0);
  duk_put_global_string(ctx, "opname");

  duk_push_c_function(ctx, bgetversion, 0);
  duk_put_global_string(ctx, "getversion");

  duk_push_c_function(ctx, bsetaction, 1);
  duk_put_global_string(ctx, "setaction");

  duk_push_c_function(ctx, bgetactions, 0);
  duk_put_global_string(ctx, "getactions");

  duk_push_c_function(ctx, bgetusernameo, 1);
  duk_put_global_string(ctx, "getusernameo");

  duk_push_c_function(ctx, bsetpasswordo, 2);
  duk_put_global_string(ctx, "setpasswordo");

  duk_push_c_function(ctx, bsetpassword, 1);
  duk_put_global_string(ctx, "setpassword");

  duk_push_c_function(ctx, bcheckpass, 1);
  duk_put_global_string(ctx, "checkpassword");

  duk_push_c_function(ctx, bisvisible, 0);
  duk_put_global_string(ctx, "isvisible");

  duk_push_c_function(ctx, breadmsg, 2);
  duk_put_global_string(ctx, "readmsg");

  duk_push_c_function(ctx, bpostmsg, 5);
  duk_put_global_string(ctx, "postmsg");

  duk_push_c_function(ctx, bsendfile, 1);
  duk_put_global_string(ctx, "sendfile");

  duk_push_c_function(ctx, bgetfilearea, 0);
  duk_put_global_string(ctx, "getfilearea");

  duk_push_c_function(ctx, bselectfilearea, 0);
  duk_put_global_string(ctx, "selectfilearea");

  duk_push_c_function(ctx, blistfiles, 0);
  duk_put_global_string(ctx, "listfiles");

  duk_push_c_function(ctx, bdownload, 0);
  duk_put_global_string(ctx, "download");

  duk_push_c_function(ctx, bcleartagged, 0);
  duk_put_global_string(ctx, "cleartagged");

  duk_push_c_function(ctx, blisttaggedfiles, 0);
  duk_put_global_string(ctx, "listtagged");

  duk_push_c_function(ctx, bupload, 0);
  duk_put_global_string(ctx, "upload");

  duk_push_c_function(ctx, beditsig, 0);
  duk_put_global_string(ctx, "editsig");

  if (duk_pcompile_string(ctx, 0, buffer.str().c_str()) != 0) {
    n->log->log(LOG_ERROR, "compile failed: %s", duk_safe_to_string(ctx, -1));
    return -1;
  }

  if (duk_pcall(ctx, 0) != DUK_EXEC_SUCCESS) {
    n->log->log(LOG_ERROR, "script error: %s", duk_safe_to_string(ctx, -1));
  }

  duk_destroy_heap(ctx);

  return 0;
}
