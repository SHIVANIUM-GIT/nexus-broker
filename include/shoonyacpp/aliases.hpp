#pragma once
#include <cstdint>
#include <string_view>

enum class ProductType : char {
  Delivery = 'C',
  Intraday = 'I',
  Normal = 'M',
  CF = 'M'
};

enum class FeedType : uint8_t { TOUCHLINE = 1, SNAPQUOTE = 2 };

enum class BuyOrSell : char { Buy = 'B', Sell = 'S' };

enum class Option : char { CE = 'C', PE = 'P' };

enum class PriceType : uint8_t { Market, Limit, StopLossLimit, StopLossMarket };

constexpr std::string_view to_string(PriceType type) {
  switch (type) {
  case PriceType::Market:
    return "MKT";
  case PriceType::Limit:
    return "LMT";
  case PriceType::StopLossLimit:
    return "SL-LMT";
  case PriceType::StopLossMarket:
    return "SL-MKT";
  default:
    return "";
  }
}

namespace shoonyacpp {
using ::BuyOrSell;
using ::FeedType;
using ::PriceType;
using ::ProductType;
using ::to_string;
} // namespace shoonyacpp
