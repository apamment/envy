#include <fstream>
#include <sstream>
#include "Script.h"
#include "Node.h"
#include "duktape.h"
#include "aixlog.hpp"

static void my_fatal(void *udata, const char *msg) {
    Node *n = (Node *)udata;

    n->bprintf("JS Error: %s\r\n", (msg ? msg : "no message"));
    n->disconnect();
}

static Node* get_node(duk_context *ctx) {
    duk_memory_functions funcs;
    duk_get_memory_functions(ctx, &funcs);
    return (Node *)funcs.udata;
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

int Script::run(Node *n, std::string filename) {
    std::ifstream t(filename);
    std::stringstream buffer;

    if (t.is_open()) {
        buffer << t.rdbuf();
    } else {
        return -1;
    }

    LOG(INFO) << "Executing " << filename;

    duk_context *ctx = duk_create_heap(NULL, NULL, NULL, (void *)n, my_fatal);

    duk_push_c_function(ctx, bprint, 1);
    duk_put_global_string(ctx, "print");

    duk_push_c_function(ctx, bgetch, 0);
    duk_put_global_string(ctx, "getch");

    duk_push_c_function(ctx, bgetstr, 1);
    duk_put_global_string(ctx, "gets");

    duk_pcompile_string(ctx, 0, buffer.str().c_str());

    duk_pcall(ctx, 0);

    duk_destroy_heap(ctx);

    return 0;
}
