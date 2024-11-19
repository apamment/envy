#pragma once

#include <string>
#include <vector>

class Config;
class Node;
class User;

class Door {
public:
  static void createDropfiles(Node *n);
  static bool runExternal(Node *n, std::string command, std::vector<std::string> args, bool raw);
  Door();
  ~Door();
};
