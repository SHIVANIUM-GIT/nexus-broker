#include "engine/OptionSymbolManager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <ctime>
#include <set>

namespace nexus::engine {

OptionSymbolManager::OptionSymbolManager(const std::string& symbol) 
    : symbol_(symbol), exp_year_(0), exp_month_(0), exp_day_(0) {}

// Helper to convert DD-MMM-YYYY to time_t for comparison
static std::time_t parse_date(const std::string& date_str) {
    std::tm tm = {};
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    
    if (date_str.size() < 11) return 0;
    
    tm.tm_mday = std::stoi(date_str.substr(0, 2));
    std::string mon_str = date_str.substr(3, 3);
    tm.tm_year = std::stoi(date_str.substr(7, 4)) - 1900;
    
    for (int i = 0; i < 12; ++i) {
        if (mon_str == months[i]) {
            tm.tm_mon = i;
            break;
        }
    }
    
    return std::mktime(&tm);
}

bool OptionSymbolManager::initialize() {
    // Download and extract if NFO_symbols.txt doesn't exist
    std::ifstream check("NFO_symbols.txt");
    if (!check.good()) {
        std::cout << "[INFO] NFO_symbols.txt not found. Downloading from Shoonya...\n";
        int res = std::system("wget -q https://api.shoonya.com/NFO_symbols.txt.zip -O NFO_symbols.txt.zip && unzip -q -o NFO_symbols.txt.zip");
        if (res != 0) {
            std::cerr << "[ERROR] Failed to download or unzip NFO symbols.\n";
            return false;
        }
    }
    check.close();

    std::ifstream file("NFO_symbols.txt");
    if (!file.is_open()) return false;

    std::string line;
    std::getline(file, line); // Skip header
    
    std::vector<std::string> rows;
    std::set<std::string> expiries;
    
    std::cout << "[INFO] Parsing NFO symbols for " << symbol_ << "...\n";
    
    // We expect columns: Exchange,Token,LotSize,Symbol,TradingSymbol,Expiry,Instrument,OptionType,StrikePrice,TickSize
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string exch, token, lot, sym, tsym, expiry, inst, opt_type, strike_str;
        
        std::getline(ss, exch, ',');
        std::getline(ss, token, ',');
        std::getline(ss, lot, ',');
        std::getline(ss, sym, ',');
        std::getline(ss, tsym, ',');
        std::getline(ss, expiry, ',');
        std::getline(ss, inst, ',');
        std::getline(ss, opt_type, ',');
        std::getline(ss, strike_str, ',');
        
        if (sym == symbol_ && (inst == "OPTIDX" || inst == "OPTSTK")) {
            expiries.insert(expiry);
            rows.push_back(line);
        }
    }
    
    if (expiries.empty()) {
        std::cerr << "[ERROR] No options found for symbol: " << symbol_ << "\n";
        return false;
    }
    
    // Sort expiries to find the nearest
    std::vector<std::string> sorted_expiries(expiries.begin(), expiries.end());
    std::sort(sorted_expiries.begin(), sorted_expiries.end(), [](const std::string& a, const std::string& b) {
        return parse_date(a) < parse_date(b);
    });
    
    nearest_expiry_ = sorted_expiries.front();
    std::cout << "[INFO] Nearest Expiry Identified: " << nearest_expiry_ << "\n";
    
    // Parse expiry date for Greeks calculations
    exp_day_ = std::stoi(nearest_expiry_.substr(0, 2));
    exp_year_ = std::stoi(nearest_expiry_.substr(7, 4));
    std::string mon_str = nearest_expiry_.substr(3, 3);
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    for (int i = 0; i < 12; ++i) {
        if (mon_str == months[i]) {
            exp_month_ = i + 1;
            break;
        }
    }
    
    // Build map for nearest expiry
    for (const auto& r : rows) {
        std::stringstream ss(r);
        std::string exch, token, lot, sym, tsym, expiry, inst, opt_type, strike_str;
        
        std::getline(ss, exch, ',');
        std::getline(ss, token, ',');
        std::getline(ss, lot, ',');
        std::getline(ss, sym, ',');
        std::getline(ss, tsym, ',');
        std::getline(ss, expiry, ',');
        std::getline(ss, inst, ',');
        std::getline(ss, opt_type, ',');
        std::getline(ss, strike_str, ',');
        
        if (expiry == nearest_expiry_ && !strike_str.empty()) {
            double strike = std::stod(strike_str);
            char type = (opt_type == "CE") ? 'C' : 'P';
            token_map_[{strike, type}] = "NFO|" + token;
        }
    }
    
    std::cout << "[INFO] Loaded " << token_map_.size() << " tokens for expiry " << nearest_expiry_ << "\n";
    return true;
}

std::string OptionSymbolManager::get_token(double strike, char type) const {
    auto it = token_map_.find({strike, type});
    if (it != token_map_.end()) {
        return it->second;
    }
    return "";
}

std::tuple<int, int, int> OptionSymbolManager::get_nearest_expiry_date() const {
    return {exp_year_, exp_month_, exp_day_};
}

std::string OptionSymbolManager::get_nearest_expiry_str() const {
    return nearest_expiry_;
}

} // namespace nexus::engine
