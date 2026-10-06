#pragma once
#include <cstdint>
#include <string>

namespace shoonyacpp {
struct OptionChainItem {
  std::string trading_symbol;
  double strike_price;
  std::string option_type;
  std::string token;
  double last_price = 0.0;
  int64_t open_interest = 0;
};
} // namespace shoonyacpp
