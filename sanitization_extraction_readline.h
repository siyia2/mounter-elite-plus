// sanitization_readline.h

#ifndef SANITIZATION_READLINE_H
#define SANITIZATION_READLINE_H
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <memory>
#include <readline/readline.h>
#include <readline/history.h>
#include <string>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

std::string shell_escape(const std::string& s);
std::pair<std::string, std::string> extractDirectoryAndFilename(const std::string& path);
std::string readInputLine(const std::string& prompt);
#endif // SANITIZATION_READLINE_H