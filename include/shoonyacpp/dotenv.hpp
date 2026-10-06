#pragma once

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace shoonyacpp {

inline void load_dotenv(const std::string &filepath = ".env") {
  std::ifstream file(filepath);

  if (!file.is_open()) {
    throw std::runtime_error("Could not open " + filepath);
  }

  std::string line;
  while (std::getline(file, line)) {
    // Strip comments
    auto comment_pos = line.find('#');
    if (comment_pos != std::string::npos) {
      line = line.substr(0, comment_pos);
    }

    size_t delimiter_pos = line.find('=');
    if (delimiter_pos != std::string::npos) {
      std::string key = line.substr(0, delimiter_pos);
      std::string value = line.substr(delimiter_pos + 1);

      // Trim whitespace from key
      key.erase(0, key.find_first_not_of(" \t\r\n"));
      key.erase(key.find_last_not_of(" \t\r\n") + 1);

      // Trim whitespace from value
      value.erase(0, value.find_first_not_of(" \t\r\n"));
      value.erase(value.find_last_not_of(" \t\r\n") + 1);

      // Strip surrounding quotes
      if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                                (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
      }

      if (!key.empty()) {
        setenv(key.c_str(), value.c_str(), 1);
      }
    }
  }
}

inline std::string get_env_var(const std::string &key,
                               const std::string &default_val = "") {
  const char *val = std::getenv(key.c_str());
  return val ? std::string(val) : default_val;
}

} // namespace shoonyacpp
