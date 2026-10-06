#include "oms/OrderManager.hpp"
#include <chrono>

namespace nexus::oms {

OrderManager::OrderManager(shoonyacpp::NorenRestApi &api,
                           const RiskLimits &limits)
    : api_(api), risk_manager_(limits) {}

uint64_t OrderManager::submit_order(const std::string &exchange,
                                    const std::string &symbol,
                                    shoonyacpp::BuyOrSell side,
                                    shoonyacpp::PriceType price_type,
                                    int32_t quantity, double price) {
  uint64_t client_id = next_client_id_.fetch_add(1, std::memory_order_relaxed);

  ManagedOrder order{};
  order.client_order_id = client_id;
  order.exchange = exchange;
  order.trading_symbol = symbol;
  order.side = side;
  order.price_type = price_type;
  order.total_qty = quantity;
  order.price = price;
  order.status = OrderStatus::Created;
  order.created_time = std::chrono::high_resolution_clock::now();
  order.updated_time = order.created_time;

  // 1. Pre-trade Risk Validation (RMS)
  std::string reject_reason;
  if (!risk_manager_.validate_order(order, reject_reason)) {
    order.status = OrderStatus::Rejected;
    order.reject_reason = reject_reason;
    std::lock_guard<std::mutex> lock(mutex_);
    orders_by_client_id_[client_id] = order;
    return client_id;
  }

  // 2. Mark in-flight
  order.status = OrderStatus::PendingNew;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    orders_by_client_id_[client_id] = order;
  }

  // 3. Dispatch to Broker REST Gateway
  shoonyacpp::Order broker_order{};
  broker_order.exchange = exchange;
  broker_order.trading_symbol = symbol;
  broker_order.side = side;
  broker_order.price_type = price_type;
  broker_order.quantity = quantity;
  broker_order.price = price;
  broker_order.remarks = "CLID:" + std::to_string(client_id);

  bool ok = api_.place_order(broker_order);

  // 4. Update in-memory state based on gateway dispatch
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = orders_by_client_id_.find(client_id);
    if (it != orders_by_client_id_.end()) {
      if (ok) {
        it->second.status = OrderStatus::Open;
      } else {
        it->second.status = OrderStatus::Rejected;
        it->second.reject_reason = "Rejected by broker or network error";
      }
      it->second.updated_time = std::chrono::high_resolution_clock::now();
    }
  }

  return client_id;
}

bool OrderManager::cancel_order(uint64_t client_order_id) {
  std::string broker_order_no;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = orders_by_client_id_.find(client_order_id);
    if (it == orders_by_client_id_.end()) {
      return false;
    }
    if (it->second.status != OrderStatus::Open &&
        it->second.status != OrderStatus::PartiallyFilled) {
      return false;
    }
    broker_order_no = it->second.broker_order_no;
    it->second.status = OrderStatus::PendingCancel;
    it->second.updated_time = std::chrono::high_resolution_clock::now();
  }

  if (broker_order_no.empty()) {
    return false;
  }

  bool ok = api_.cancel_order(broker_order_no);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = orders_by_client_id_.find(client_order_id);
    if (it != orders_by_client_id_.end()) {
      if (ok) {
        it->second.status = OrderStatus::Cancelled;
      } else {
        it->second.status = OrderStatus::Open; // Revert if cancel failed
      }
      it->second.updated_time = std::chrono::high_resolution_clock::now();
    }
  }

  return ok;
}

std::optional<ManagedOrder> OrderManager::get_order(uint64_t client_order_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = orders_by_client_id_.find(client_order_id);
  if (it != orders_by_client_id_.end()) {
    return it->second;
  }
  return std::nullopt;
}

void OrderManager::on_trade_fill(const std::string &broker_order_no,
                                 int32_t fill_qty, double fill_price) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it_id = broker_to_client_id_map_.find(broker_order_no);
  if (it_id == broker_to_client_id_map_.end()) {
    return;
  }

  auto it_order = orders_by_client_id_.find(it_id->second);
  if (it_order == orders_by_client_id_.end()) {
    return;
  }

  auto &ord = it_order->second;
  double old_cost = ord.avg_fill_price * ord.filled_qty;
  ord.filled_qty += fill_qty;
  if (ord.filled_qty > 0) {
    ord.avg_fill_price =
        (old_cost + (fill_price * fill_qty)) / ord.filled_qty;
  }

  if (ord.filled_qty >= ord.total_qty) {
    ord.status = OrderStatus::Filled;
  } else {
    ord.status = OrderStatus::PartiallyFilled;
  }
  ord.updated_time = std::chrono::high_resolution_clock::now();
}

std::vector<ManagedOrder> OrderManager::get_open_orders() {
  std::vector<ManagedOrder> open_orders;
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &[id, order] : orders_by_client_id_) {
    if (order.status == OrderStatus::Open ||
        order.status == OrderStatus::PartiallyFilled ||
        order.status == OrderStatus::PendingNew) {
      open_orders.push_back(order);
    }
  }
  return open_orders;
}

} // namespace nexus::oms
