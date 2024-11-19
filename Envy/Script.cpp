#include <fstream>
#include <sstream>
#include <sqlite3.h>
#include "Script.h"
#include "Node.h"
#include "User.h"
#include "duktape.h"
#include "MessageBase.h"

struct script_data {
    Node *n;
    std::string script;
};

bool Script::open_database(Node *n, sqlite3 **db) {
  static const char *create_details_sql = "CREATE TABLE IF NOT EXISTS details(script TEXT COLLATE NOCASE, attrib TEXT COLLATE NOCASE, value TEXT COLLATE NOCASE);";

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

static Node* get_node(duk_context *ctx) {
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

static duk_ret_t bselectarea(duk_context *ctx) {
    Node *n = get_node(ctx);

    n->select_msg_base();

    return 0;
}

static duk_ret_t bgetarea(duk_context *ctx) {
    Node *n = get_node(ctx);

    duk_push_string(ctx, n->get_curr_msgbase()->name.c_str());

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

    duk_push_c_function(ctx, bgetattr, 2);
    duk_put_global_string(ctx, "getattr");

    duk_push_c_function(ctx, bputattr, 2);
    duk_put_global_string(ctx, "putattr");

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

    duk_push_c_function(ctx, bselectarea, 0);
    duk_put_global_string(ctx, "selectarea");

    duk_push_c_function(ctx, bgetarea, 0);
    duk_put_global_string(ctx, "getarea");

    duk_push_c_function(ctx, bpause, 0);
    duk_put_global_string(ctx, "pause");

    duk_push_c_function(ctx, bscanbases, 0);
    duk_put_global_string(ctx, "scanbases");

    duk_push_c_function(ctx, brundoor, 0);
    duk_put_global_string(ctx, "rundoor");

    if (duk_pcompile_string(ctx, 0, buffer.str().c_str()) != 0) {
        n->log->log(LOG_ERROR, "compile failed: %s", duk_safe_to_string(ctx, -1));
        return -1;
    }

    duk_pcall(ctx, 0);

    duk_destroy_heap(ctx);

    return 0;
}
