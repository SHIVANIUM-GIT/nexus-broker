#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "OrderState.hpp"
#include "RiskManager.hpp"
#include "shoonyacpp/api.hpp"

namespace nexus::oms {

class OrderManager {
public:
  explicit OrderManager(shoonyacpp::NorenRestApi &api,
                        const RiskLimits &limits = RiskLimits{});

  // Strategy calls this to place an order
  uint64_t submit_order(const std::string &exchange, const std::string &symbol,
                        shoonyacpp::BuyOrSell side,
                        shoonyacpp::PriceType price_type, int32_t quantity,
                        double price);

  // Cancel order by local internal ID
  bool cancel_order(uint64_t client_order_id);

  // Instant local memory lookup
  std::optional<ManagedOrder> get_order(uint64_t client_order_id);

  // Reconcile with exchange trade book fills
  void on_trade_fill(const std::string &broker_order_no, int32_t fill_qty,
                     double fill_price);

  // Get all active open orders
  std::vector<ManagedOrder> get_open_orders();

private:
  shoonyacpp::NorenRestApi &api_;
  RiskManager risk_manager_;

  std::atomic<uint64_t> next_client_id_{1};

  // Thread-safe in-memory maps
  mutable std::mutex mutex_;
  std::unordered_map<uint64_t, ManagedOrder> orders_by_client_id_;
  std::unordered_map<std::string, uint64_t> broker_to_client_id_map_;
};

} // namespace nexus::oms
