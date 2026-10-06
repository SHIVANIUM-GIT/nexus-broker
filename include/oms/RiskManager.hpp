#pragma once

#include "OrderState.hpp"
#include <string>

namespace nexus::oms {

struct RiskLimits {
  int32_t max_quantity_per_order = 1800;
  double max_value_per_order = 200000.0;
  int32_t max_open_orders = 50;
};

class RiskManager {
public:
  explicit RiskManager(const RiskLimits &limits = RiskLimits{});

  bool validate_order(const ManagedOrder &order, std::string &out_reason);

  void set_limits(const RiskLimits &limits) { limits_ = limits; }
  const RiskLimits &get_limits() const { return limits_; }

private:
  RiskLimits limits_;
};

} // namespace nexus::oms
