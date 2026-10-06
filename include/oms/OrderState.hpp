#pragma once

#include "shoonyacpp/aliases.hpp"
#include <chrono>
#include <cstdint>
#include <string>

namespace nexus::oms {

// 1. The formal states an order moves through
enum class OrderStatus : uint8_t {
  Created,         // Instantiated locally in engine
  PendingNew,      // Sent to broker, waiting for ACK
  Open,            // Active on exchange order book
  PartiallyFilled, // Some quantity matched, remainder waiting
  Filled,          // 100% executed
  PendingCancel,   // Cancel request sent, waiting for exchange ACK
  Cancelled,       // Successfully cancelled
  Rejected         // Rejected by Risk Engine or Exchange
};

// Helper to print human-readable status
constexpr std::string_view to_string(OrderStatus status) {
  switch (status) {
  case OrderStatus::Created:
    return "CREATED";
  case OrderStatus::PendingNew:
    return "PENDING_NEW";
  case OrderStatus::Open:
    return "OPEN";
  case OrderStatus::PartiallyFilled:
    return "PARTIALLY_FILLED";
  case OrderStatus::Filled:
    return "FILLED";
  case OrderStatus::PendingCancel:
    return "PENDING_CANCEL";
  case OrderStatus::Cancelled:
    return "CANCELLED";
  case OrderStatus::Rejected:
    return "REJECTED";
  default:
    return "UNKNOWN";
  }
}

// 2. The Internal Order object tracked in memory
struct ManagedOrder {
  uint64_t client_order_id = 0; // Local unique ID (1, 2, 3...)
  std::string broker_order_no;  // Shoonya's norenordno (once ACKed)

  std::string trading_symbol;
  std::string exchange;
  shoonyacpp::BuyOrSell side;
  shoonyacpp::PriceType price_type;
  shoonyacpp::ProductType product_type = shoonyacpp::ProductType::Intraday;

  int32_t total_qty = 0;
  int32_t filled_qty = 0;
  double price = 0.0;
  double avg_fill_price = 0.0;

  OrderStatus status = OrderStatus::Created;
  std::string reject_reason;

  std::chrono::high_resolution_clock::time_point created_time;
  std::chrono::high_resolution_clock::time_point updated_time;
};

} // namespace nexus::oms
