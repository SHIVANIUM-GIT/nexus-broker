#pragma once
#include <cstdint>
#include <string>

namespace shoonyacpp {

struct OrderBook {
  std::string order_no;
  std::string trading_symbol;
  std::string exchange;
  std::string side;
  std::string product;
  std::string price_type;
  std::string status;
  int32_t quantity = 0;
  double price = 0.0;
  int32_t filled_qty = 0;
  double avg_price = 0;
  std::string rejection_reason;
  std::string order_time;
  std::string remarks;
};
} // namespace shoonyacpp
