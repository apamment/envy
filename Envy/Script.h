#pragma once

#include <string>

class Node;


class Script {
public:
    static int run(Node *n, std::string filename);
};
