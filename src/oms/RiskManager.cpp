#include "oms/RiskManager.hpp"

namespace nexus::oms {

RiskManager::RiskManager(const RiskLimits &limits) : limits_(limits) {}

bool RiskManager::validate_order(const ManagedOrder &order,
                                 std::string &out_reason) {
  // 1. Quantity check (positive and within freeze limit)
  if (order.total_qty <= 0) {
    out_reason = "Order quantity must be positive";
    return false;
  }
  if (order.total_qty > limits_.max_quantity_per_order) {
    out_reason = "Order quantity " + std::to_string(order.total_qty) +
                 " exceeds max allowed per order (" +
                 std::to_string(limits_.max_quantity_per_order) + ")";
    return false;
  }

  // 2. Notional order value check (price * quantity)
  double notional_value = order.price * order.total_qty;
  if (notional_value > limits_.max_value_per_order) {
    out_reason = "Order value " + std::to_string(notional_value) +
                 " exceeds max allowed value (" +
                 std::to_string(limits_.max_value_per_order) + ")";
    return false;
  }

  // 3. Symbol & Exchange check
  if (order.trading_symbol.empty()) {
    out_reason = "Trading symbol cannot be empty";
    return false;
  }
  if (order.exchange.empty()) {
    out_reason = "Exchange cannot be empty";
    return false;
  }

  return true;
}

} // namespace nexus::oms
