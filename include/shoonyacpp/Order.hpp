#pragma once
#include <string>
#include <optional>
#include "aliases.hpp"

namespace shoonyacpp {

struct Order {
    BuyOrSell side;
    std::string trading_symbol;
    std::string exchange;
    int quantity;
    PriceType price_type;
    std::optional<double> price;
    std::optional<double> trigger_price;
    std::optional<std::string> remarks;
};

} // namespace shoonyacpp
