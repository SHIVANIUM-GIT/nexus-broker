#pragma once

#include <string>
#include <map>
#include <vector>
#include <tuple>
#include <chrono>

namespace nexus::engine {

class OptionSymbolManager {
public:
    OptionSymbolManager(const std::string& symbol);
    
    // Downloads and parses the master file
    bool initialize();

    // Gets the mapped token for a strike and type
    std::string get_token(double strike, char type) const;

    // Gets the parsed nearest expiry date (year, month, day)
    std::tuple<int, int, int> get_nearest_expiry_date() const;
    
    // Gets the string representation of the nearest expiry
    std::string get_nearest_expiry_str() const;

private:
    std::string symbol_;
    std::string nearest_expiry_;
    int exp_year_, exp_month_, exp_day_;
    
    // Maps {strike, type} -> Token
    std::map<std::pair<double, char>, std::string> token_map_;
};

} // namespace nexus::engine
