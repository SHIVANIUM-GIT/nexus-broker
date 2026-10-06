#pragma once
#include <cstdint>
#include <cstdlib>
#include <string>

namespace shoonyacpp {
struct TradeBook {
  std::string order_no;
  std::string exchange;
  std::string trading_symbol;
  std::string side;
  std::string product;
  std::string price_type;
  std::string fill_id;
  std::string fill_time;
  int32_t fill_qty = 0;
  double fill_price = 0.0;
  int32_t order_qty = 0;
  int32_t total_filled_qty = 0;
  double avg_price = 0.0;
  std::string exchange_order_id;
  std::string remarks;
};
} // namespace shoonyacpp
