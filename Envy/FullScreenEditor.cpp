#include "FullScreenEditor.h"
#include "Node.h"
#include <sstream>
#include <ctime>
#include <iomanip>
#include <string>
#include <vector>

class FullScreenBuffer {
public:
  FullScreenBuffer(Node *n, std::vector<std::string> *initialbuffer) {
    this->n = n;
    if (initialbuffer != nullptr && initialbuffer->size() > 0) {
      for (size_t i = 0; i < initialbuffer->size(); i++) {
        std::stringstream sanss;
        for (size_t j = 0; j < initialbuffer->at(i).size(); j++) {
          if (initialbuffer->at(i).at(j) != '\n') {
            sanss << initialbuffer->at(i).at(j);
          }
        }

        lines.push_back(sanss.str());
      }
    } else {
      lines.push_back(std::string(""));
    }
  }

  void insert_content(std::vector<std::string> content) {
    for (size_t i = 0; i < content.size(); i++) {
      lines.insert(lines.begin() + line_at + i, content.at(i));
    }

    rewrap(line_at);

    line_at += content.size();

    if (line_at > top + (int)18) {
      top += line_at - (top + (int)18) + 1;
    }
  }

  std::vector<std::string> return_body() { return lines; }

  void update_cursor() {
    if (line_at < top) {
      top--;
    }

    if (line_at > top + (int)18) {
      top++;
    }

    n->bprintf_nc("\x1b[%d;%dH", line_at - top + 5, col_at + 1);
  }

  void refresh_line(int lineno) {
    if (lineno >= (int)top && lineno <= (int)top + (int)18) {
      n->bprintf_nc("\x1b[%d;%dH\x1b[0m%s\x1b[K", lineno - top + 5, 1, lines.at(lineno).c_str());
    }
  }

  void refresh_down(int start) {
    for (size_t i = start; i < lines.size(); i++) {
      refresh_line(i);
      if (i > top + 18) {
        break;
      }
    }
    for (size_t i = lines.size(); i <= top + 18; i++) {
      n->bprintf_nc("\x1b[%d;%dH\x1b[0;31m\x1b[K", i - top + 5, 1);
    }
  }

  void refresh_screen() { refresh_down(top); }

  void move_cursor_up() {
    if (line_at == 0)
      return;
    line_at--;

    if (col_at > lines.at(line_at).size()) {
      col_at = lines.at(line_at).size();
    }

    if (line_at < top) {
      top--;
      refresh_screen();
    }

    update_cursor();
  }

  void move_cursor_down() {
    if (line_at == lines.size() - 1)
      return;
    line_at++;

    if (col_at > lines.at(line_at).size()) {
      col_at = lines.at(line_at).size();
    }

    if (line_at > top + 18) {
      top++;
      refresh_screen();
    }

    update_cursor();
  }

  void move_cursor_end() {
    col_at = lines.at(line_at).size();
    update_cursor();
  }

  void move_cursor_home() {
    col_at = 0;
    update_cursor();
  }

  void move_cursor_left() {
    if (col_at == 0) {
      if (line_at > 0) {
        col_at = lines.at(line_at - 1).size();
        line_at--;
        update_cursor();
      }
      return;
    }
    col_at--;
    update_cursor();
  }
  void move_cursor_right() {
    if (col_at == lines.at(line_at).size()) {
      if (line_at < lines.size() - 1) {
        line_at++;
        col_at = 0;
        update_cursor();
      }
      return;
    }

    col_at++;
    update_cursor();
  }

  void delete_line() {
    if (lines.size() > 1) {
      if (line_at == lines.size() - 1) { // on the last line
        lines.erase(lines.begin() + line_at);
        line_at--;
      } else {
        lines.erase(lines.begin() + line_at);
      }
    } else {
      lines.clear();
      lines.push_back("");
    }
    if (col_at > lines.at(line_at).size()) {
      col_at = lines.at(line_at).size();
    }
    refresh_screen();
    update_cursor();
  }

  void insert_line() {
    int old_line_at = line_at;
    if (col_at == 0) {
      lines.insert(lines.begin() + line_at, std::string());
      line_at++;
    } else if (col_at == lines.at(line_at).size()) {
      line_at++;
      col_at = 0;
      lines.insert(lines.begin() + line_at, std::string());
    } else {
      std::string oldline = lines.at(line_at);
      lines.erase(lines.begin() + line_at);
      lines.insert(lines.begin() + line_at, oldline.substr(0, col_at));
      lines.insert(lines.begin() + line_at + 1, oldline.substr(col_at));
      line_at++;
      col_at = 0;
    }
    if (line_at > top + 18) {
      top++;
      refresh_screen();
    } else {
      refresh_down(old_line_at);
    }
    update_cursor();
  }

  void delete_char() {
    std::stringstream ss, ss2;

    if (col_at > 0) {

      ss << lines.at(line_at).substr(0, col_at - 1);
      ss << lines.at(line_at).substr(col_at);

      lines.at(line_at) = ss.str();

      col_at--;

      if (line_at < lines.size() - 1) {
        if (lines.at(line_at + 1).size() > 0) {
          // should pull in the next line's first word?
          size_t fs = lines.at(line_at + 1).find(' ');

          if (74 - lines.at(line_at).size() <= lines.at(line_at + 1).find(' ', fs + 1)) {

            if (lines.at(line_at).size() + lines.at(line_at + 1).size() + 1 <= 74) {
              // the whole next line fits..
              ss.str("");
              if (lines.at(line_at + 1).at(0) != ' ') {
                ss << lines.at(line_at) << " " << lines.at(line_at + 1);
              } else {
                ss << lines.at(line_at) << lines.at(line_at + 1);
              }
              lines.at(line_at) = ss.str();
              lines.erase(lines.begin() + line_at + 1);
              refresh_down(line_at);
            } else {

              size_t first_space = lines.at(line_at + 1).substr(0, 74 - lines.at(line_at).size()).rfind(' ');
              if (first_space != std::string::npos) {
                if (lines.at(line_at).size() + first_space <= 74) {
                  ss.str("");
                  ss << lines.at(line_at) << " " << lines.at(line_at + 1).substr(0, first_space);
                  ss2 << lines.at(line_at + 1).substr(first_space + 1);
                  lines.at(line_at) = ss.str();
                  lines.at(line_at + 1) = ss2.str();
                  refresh_down(line_at);
                } else {
                  refresh_line(line_at);
                }
              } else {
                refresh_line(line_at);
              }
            }
          } else {
            refresh_line(line_at);
          }
        } else {
          refresh_line(line_at);
        }
      } else {
        refresh_line(line_at);
      }
    } else {
      if (line_at > 0) {
        line_at--;
        col_at = lines.at(line_at).size();

        if (line_at < lines.size() - 1) {
          if (lines.at(line_at + 1).size() > 0) {
            if (lines.at(line_at).size() + lines.at(line_at + 1).size() + 1 <= 74) {
              ss.str("");
              if (lines.at(line_at + 1).at(0) != ' ') {
                ss << lines.at(line_at) << " " << lines.at(line_at + 1);
              } else {
                ss << lines.at(line_at) << lines.at(line_at + 1);
              }
              lines.at(line_at) = ss.str();
              lines.erase(lines.begin() + line_at + 1);
              refresh_down(line_at);
            } else {
              size_t first_space = lines.at(line_at + 1).substr(0, 74 - lines.at(line_at).size()).rfind(' ');
              if (first_space != std::string::npos) {
                if (lines.at(line_at).size() + first_space <= 74) {
                  ss.str("");
                  ss << lines.at(line_at) << lines.at(line_at + 1).substr(0, first_space);
                  ss2 << lines.at(line_at + 1).substr(first_space + 1);
                  lines.at(line_at) = ss.str();
                  lines.at(line_at + 1) = ss2.str();
                  refresh_down(line_at);
                } else {
                  refresh_line(line_at);
                }
              } else {
                refresh_line(line_at);
              }
            }
          } else {
            lines.erase(lines.begin() + line_at + 1);
            refresh_down(line_at);
          }
        } else {
          refresh_line(line_at);
        }
      }
    }
    update_cursor();
  }

  void rewrap(size_t start) {
    bool done = false;

    while (!done) {
      size_t lineno;
      for (lineno = start; lineno < lines.size(); lineno++) {
        if (lines.at(lineno).size() > 74) {
          if (lineno == lines.size() - 1 || lines.at(lineno).size() == 0) {
            // insert line
            std::stringstream ss1;
            std::stringstream ss2;

            size_t last_space = lines.at(lineno).substr(0, 74).rfind(' ');
            if (last_space == std::string::npos) {
              ss1 << lines.at(lineno).substr(0, 74);
              ss2 << lines.at(lineno).substr(74);
            } else {
              ss1 << lines.at(lineno).substr(0, last_space);
              ss2 << lines.at(lineno).substr(last_space + 1);
            }

            lines.at(lineno) = ss1.str();
            lines.insert(lines.begin() + lineno + 1, ss2.str());
            done = true;
          } else {
            // insert leftover to next line
            std::stringstream ss1;
            std::stringstream ss2;

            size_t last_space = lines.at(lineno).substr(0, 74).rfind(' ');
            if (last_space == std::string::npos) {
              ss1 << lines.at(lineno).substr(0, 74);
              ss2 << lines.at(lineno).substr(74) << lines.at(lineno + 1);
            } else {
              ss1 << lines.at(lineno).substr(0, last_space);
              ss2 << lines.at(lineno).substr(last_space + 1) << ' ' << lines.at(lineno + 1);
            }

            lines.at(lineno) = ss1.str();
            lines.at(lineno + 1) = ss2.str();
          }
          break;
        }
      }
      if (lineno == lines.size()) {
        done = true;
      }
    }
  }

  void add_char(char c) {
    std::stringstream ss, ss2;

    size_t last_space = lines.at(line_at).rfind(' ');

    if (lines.at(line_at).size() >= 74) {
      if (last_space != std::string::npos) {
        if (last_space < col_at) {
          ss << lines.at(line_at).substr(0, last_space);
          ss2 << lines.at(line_at).substr(last_space + 1, col_at - (last_space + 1)) << c << lines.at(line_at).substr(col_at);

          lines.at(line_at) = ss.str();

          if (line_at < lines.size() - 1 && lines.at(line_at + 1).size() > 0) {
            std::stringstream ss3;
            ss3 << ss2.str() << ' ' << lines.at(line_at + 1);

            lines.at(line_at + 1) = ss3.str();
            rewrap(line_at + 1);
          } else {
            lines.insert(lines.begin() + line_at + 1, ss2.str());
          }
          col_at -= last_space;
          line_at++;
        } else {
          ss << lines.at(line_at).substr(0, col_at) << c << lines.at(line_at).substr(col_at, last_space - col_at);
          ss2 << lines.at(line_at).substr(last_space + 1);

          lines.at(line_at) = ss.str();

          if (line_at < lines.size() - 1 && lines.at(line_at + 1).size() > 0) {
            std::stringstream ss3;

            ss3 << ss2.str() << ' ' << lines.at(line_at + 1);

            lines.at(line_at + 1) = ss3.str();
            rewrap(line_at + 1);
          } else {
            lines.insert(lines.begin() + line_at + 1, ss2.str());
          }

          col_at++;
        }
      } else {
        if (col_at > 74) {
          ss << lines.at(line_at).substr(0, 74);
          ss2 << lines.at(line_at).substr(74, col_at - 74) << c;
          lines.at(line_at) = ss.str();
          lines.insert(lines.begin() + line_at + 1, ss2.str());
          col_at = col_at - 74;
          line_at++;
        } else {
          ss << lines.at(line_at).substr(0, col_at) << c << lines.at(line_at).substr(col_at, 74 - col_at);
          ss2 << lines.at(line_at).substr(74);
          lines.at(line_at) = ss.str();
          lines.insert(lines.begin() + line_at + 1, ss2.str());
        }
      }
      if (line_at > 0) {
        if (line_at > top + 18) {
          top++;
          refresh_screen();
        } else {
          refresh_down(line_at - 1);
        }
      } else {
        refresh_down(line_at);
      }
      update_cursor();
    } else {
      ss << lines.at(line_at).substr(0, col_at) << c << lines.at(line_at).substr(col_at);
      lines.at(line_at) = ss.str();
      col_at++;
      refresh_line(line_at);
      update_cursor();
    }
  }

private:
  Node *n;
  size_t col_at = 0;
  size_t line_at = 0;
  std::vector<std::string> lines;
  size_t top = 0;
};

FullScreenEditor::FullScreenEditor(Node *n, std::string to, std::string subject, std::vector<std::string> *quotelines, std::vector<std::string> *body) {
  reply = true;
  this->to = to;
  this->subject = subject;
  if (quotelines == nullptr) {
    this->quotelines = std::vector<std::string>();
  } else {
    this->quotelines = *quotelines;
  }
  initialbuffer = body;

  this->n = n;
}

std::vector<std::string> FullScreenEditor::do_quote() {
  int start = 0;
  int selected = 0;
  int preview_start = 0;

  std::vector<std::string> to_quote;

  while (true) {
    n->bprintf_nc("\x1b[s");
    gotoyx(2, 72);
    n->bprintf_nc("\x1b[1;32;45m%s\x1b[0m", timestr().c_str());
    gotoyx(3, 70);
    n->bprintf_nc("\x1b[1;32;45m%dm\x1b[0m", n->get_timeleft());
    n->bprintf_nc("\x1b[u");

    for (size_t i = 5; i <= 23; i++) {
      n->bprintf_nc("\x1b[%d;1H\x1b[K", i);
    }

    n->bprintf_nc("\x1b[12;1H\x1b[1;37;45mSelect Quote with SPACE, C to Cancel, Q to Quit\x1b[K");

    for (size_t i = preview_start; i < to_quote.size(); i++) {
      n->bprintf_nc("\x1b[%d;1H\x1b[0m%s\x1b[K", (i - preview_start) + 5, to_quote.at(i).c_str());
    }

    for (size_t i = start; i < start + 11 && i < quotelines.size(); i++) {
      if ((int)i == selected) {
        n->bprintf_nc("\x1b[%d;1H\x1b[1;47;30m%s\x1b[K\x1b[0m", (i - start) + 13, quotelines.at(i).c_str());
      } else {
        n->bprintf_nc("\x1b[%d;1H\x1b[0m%s\x1b[K", (i - start) + 13, quotelines.at(i).c_str());
      }
    }

    char c = n->getch_real(true);

    if (c == '\x1b') {
      c = n->getch();
      if (c == '[') {
        c = n->getch();
        if (c == 'A') {
          if (selected > 0) {
            selected--;
          }
        } else if (c == 'B') {
          if (selected < (int)quotelines.size() - 1) {
            selected++;
          }
        }

        if (selected < start) {
          start = selected;
        } else if (selected >= start + 11) {
          start++;
        }

        continue;
      }
    } else if (c == ' ' || c == '\r') {
      to_quote.push_back(quotelines.at(selected));
      if (selected < (int)quotelines.size() - 1) {
        selected++;
        if (selected >= start + 11) {
          start++;
        }
      }
      if (to_quote.size() - preview_start > 8 - 1) {
        preview_start++;
      }
    } else if (tolower(c) == 'q') {
      return to_quote;
    } else if (tolower(c) == 'c') {
      to_quote.clear();
      return to_quote;
    }
  }
}
/*
std::vector<std::string> FullScreenEditor::do_quote() {
        std::vector<std::string> content;
        bool stop = false;
        n->bprintf_nc("\x1b[4;1H\x1b[J");
        int lines_printed = 0;

        for (int i = 0; i < quotelines.size() && !stop; i++) {
                if (quotelines.at(i).length() > 74) {
                        n->bprintf_nc("\x1b[1;37m%4d \x1b[0m%s\r\n", i + 1, quotelines.at(i).substr(0, 74).c_str());
                        lines_printed++;
                        if (lines_printed == (n->get_term_height() - 7)) {
                                n->bprintf_nc("\x1b[1;37mPress (C) to continue, or (S) to stop\x1b[0m");
                                char c = n->getch();
                                if (tolower(c) == 's') {
                                        stop = true;
                                        break;
                                }
                                else {
                                        n->bprintf_nc("\x1b[4;1H\x1b[J");
                                        lines_printed = 0;
                                }
                        }
                        for (int z = 74; z < quotelines.at(i).length(); z += 74) {
                                int left = quotelines.at(i).length() - z;
                                if (left > 74) left = 74;
                                n->bprintf_nc("\x1b[1;37m     \x1b[0m%s\r\n", quotelines.at(i).substr(z, left).c_str());
                                lines_printed++;
                                if (lines_printed == (n->get_term_height() - 7)) {
                                        n->bprintf_nc("\x1b[1;37mPress (C) to continue, or (S) to stop\x1b[0m");
                                        char c = n->getch();
                                        if (tolower(c) == 's') {

                                                stop = true;
                                                break;
                                        }
                                        else {
                                                n->bprintf_nc("\x1b[4;1H\x1b[J");
                                                lines_printed = 0;
                                        }
                                }
                        }
                }
                else {
                        n->bprintf_nc("\x1b[1;37m%4d \x1b[0m%s\x1b[K\r\n", i + 1, quotelines.at(i).c_str());
                        lines_printed++;
                }

                if (lines_printed == (n->get_term_height() - 7)) {
                        n->bprintf_nc("\x1b[1;37mPress (C) to continue, or (S) to stop\x1b[0m");
                        char c = n->getch();

                        if (tolower(c) == 's') {
                                break;
                        }
                        else {
                                n->bprintf_nc("\x1b[4;1H\x1b[J");
                                lines_printed = 0;
                        }
                }
        }

        n->bprintf_nc("\x1b[%d;1H\x1b[K\x1b[1;37mQuote From Line #: \x1b[0m", n->get_term_height() - 3);
        std::string from = n->get_string(5, false);
        int from_num;
        try {
                from_num = std::stoi(from);
        }
        catch (std::invalid_argument  e) {
                return content;
        }

        n->bprintf_nc("\x1b[%d;1H\x1b[K\x1b[1;37m  Quote To Line #: \x1b[0m", n->get_term_height() - 2);
        std::string to = n->get_string(5, false);
        int to_num;
        try {
                to_num = std::stoi(to);
        }
        catch (std::invalid_argument  e) {
                return content;
        }

        if (from_num < 1 || to_num >= quotelines.size()) {
                return content;
        }


        for (int i = from_num - 1; i < to_num; i++) {
                content.push_back(quotelines.at(i));
        }

        return content;
}

*/

std::vector<std::string> FullScreenEditor::edit() {
  FullScreenBuffer fsb(n, initialbuffer);
  n->bprintf("\x1b[?25h");
  n->cls();
  n->putgfile("editorbd");
  gotoyx(2, 7);
  n->bprintf_nc("\x1b[1;35;45m%s", to.c_str());
  gotoyx(2, 42);
  n->bprintf_nc("\x1b[1;35;45m%s", n->get_username().c_str());
  gotoyx(3, 12);
  n->bprintf_nc("\x1b[1;35;45m%s", subject.c_str());
  
  if (n->get_node() / 100 > 0) {
    gotoyx(4, 75);
    n->bprintf_nc("\x1b[1;36;40m%d", n->get_node() / 100);
  }

  if (n->get_node() / 10 % 10 > 0) {
    gotoyx(4, 76);
    n->bprintf_nc("\x1b[1;36;40m%d", n->get_node() / 10 % 10);
  }

  gotoyx(4, 77);
  n->bprintf_nc("\x1b[1;36;40m%d", n->get_node() % 10);

  fsb.refresh_screen();
  fsb.update_cursor();

  while (true) {
    n->bprintf_nc("\x1b[s");
    gotoyx(2, 72);
    n->bprintf_nc("\x1b[1;32;45m%s\x1b[0m", timestr().c_str());
    gotoyx(3, 70);
    n->bprintf_nc("\x1b[1;32;45m%dm\x1b[0m", n->get_timeleft());
    n->bprintf_nc("\x1b[u");

    char c = n->getch_real(true);

    if (c == '\x1b') {
      c = n->getch_real(true);
      if (c == '[') {
        c = n->getch_real(true);
        if (c == 'A') {
          fsb.move_cursor_up();
        } else if (c == 'B') {
          fsb.move_cursor_down();
        } else if (c == 'C') {
          fsb.move_cursor_right();
        } else if (c == 'D') {
          fsb.move_cursor_left();
        } else if (c == 'K') {
          // END Key
          fsb.move_cursor_end();
        } else if (c == 'H') {
          fsb.move_cursor_home();
        }
        continue;
      }
    }

    if (c == '\r') {
      fsb.insert_line();
    } else if (c == '\b' || c == 127) {
      fsb.delete_char();
    } else if (c >= 32 && c <= 126) {
      fsb.add_char(c);
    } else if (c == 'y' - 'a' + 1) {
      fsb.delete_line();
    } else if (c == 'z' - 'a' + 1) {
      // ctrl-z
      n->bprintf_nc("\x1b[%d;23H\x1b[0;30;47m+--------[MENU]--------+", n->get_term_height() / 2 - 4);
      n->bprintf_nc("\x1b[%d;23H|                      |", (n->get_term_height() / 2 - 4) + 1);
      n->bprintf_nc("\x1b[%d;23H| (Q) Quote Message    |", (n->get_term_height() / 2 - 4) + 2);
      n->bprintf_nc("\x1b[%d;23H| (S) Save Message     |", (n->get_term_height() / 2 - 4) + 3);
      n->bprintf_nc("\x1b[%d;23H| (A) Abort Message    |", (n->get_term_height() / 2 - 4) + 4);
      n->bprintf_nc("\x1b[%d;23H| (C) Continue Message |", (n->get_term_height() / 2 - 4) + 5);
      n->bprintf_nc("\x1b[%d;23H|                      |", (n->get_term_height() / 2 - 4) + 6);
      n->bprintf_nc("\x1b[%d;23H+----------------------+\x1b[0m", (n->get_term_height() / 2 - 4) + 7);
      do {
        c = n->getch();
        if (tolower(c) == 's') {
          return fsb.return_body();
        } else if (tolower(c) == 'a') {
          fsb.refresh_screen();
          n->bprintf_nc("\x1b[%d;23H\x1b[1;37;41m+----[Really Abort?]---+", n->get_term_height() / 2 - 2);
          n->bprintf_nc("\x1b[%d;23H|  (Y) Yes / (N) No    |", (n->get_term_height() / 2 - 2) + 1);
          n->bprintf_nc("\x1b[%d;23H+----------------------+\x1b[0m", (n->get_term_height() / 2 - 2) + 2);

          c = n->getch();
          if (c == 'y') {
            return std::vector<std::string>();
          }
          break;
        } else if (tolower(c) == 'q') {
          if (quotelines.size() > 0) {
            fsb.insert_content(do_quote());
          }
          break;
        }
      } while (tolower(c) != 'c');
      fsb.refresh_screen();
      fsb.update_cursor();
    }
  }
}

std::string FullScreenEditor::timestr() {
  time_t now = time(NULL);
  struct tm nowtm;
  std::stringstream ss;

  localtime_r(&now, &nowtm);


  if (nowtm.tm_hour > 12) {
    ss << std::setw(2) << std::setfill(' ') << (nowtm.tm_hour - 12);
  } else if (nowtm.tm_hour == 12 || nowtm.tm_hour == 0) {
    ss << 12;
  } else {
    ss << std::setw(2) << std::setfill(' ') << nowtm.tm_hour;
  }

  ss << ":";

  ss << std::setw(2) << std::setfill('0') << nowtm.tm_min;

  if (nowtm.tm_hour > 11) {
    ss << "pm";
  } else {
    ss << "am";
  }

  return ss.str();
}

void FullScreenEditor::gotoyx(int y, int x) {
  n->bprintf_nc("\x1b[%d;%dH", y, x);
}

FullScreenEditor::~FullScreenEditor() {}
