#include "engine/RealtimeEngine.hpp"
#include <charconv>
#include <iostream>

namespace nexus::engine {

RealtimeEngine::RealtimeEngine(shoonyacpp::NorenWebsocket &ws,
                               const std::string &underlying_token,
                               int strike_interval)
    : ws_(ws), underlying_token_(underlying_token),
      strike_interval_(strike_interval), is_running_(false) {
  ws_wrapper_ = std::make_unique<WsClientWrapper>(ws_);
  md_cache_ = std::make_unique<MarketDataCache>();
  // ATM ± 5 strikes strategy, ATM ± 10 strikes buffer
  sub_mgr_ = std::make_unique<SubscriptionManager>(ws_wrapper_.get(),
                                                   strike_interval_, 5, 15);

  token_map_[underlying_token_] = {true, 0.0, ' '};
}

RealtimeEngine::~RealtimeEngine() { stop(); }

void RealtimeEngine::start(double time_to_expiry, double risk_free_rate) {
  if (is_running_)
    return;

  chain_mgr_ =
      std::make_unique<OptionChainManager>(time_to_expiry, risk_free_rate);

  if (custom_mapper_) {
    sub_mgr_->set_instrument_mapper(
        [this](double strike, const std::string &type) {
          std::string token = custom_mapper_(strike, type);
          register_option_token(token, strike, type[0]);
          return token;
        });
  } else {
    // Mock instrument mapper
    sub_mgr_->set_instrument_mapper([this](double strike,
                                           const std::string &type) {
      std::string token =
          "NFO|NIFTY_MOCK_" + std::to_string(static_cast<int>(strike)) + type;
      register_option_token(token, strike, type[0]);
      return token;
    });
  }

  ws_.set_on_tick_callback(
      [this](const std::string &tick) { this->on_tick(tick); });

  is_running_ = true;
  ws_.subscribe(underlying_token_);
}

void RealtimeEngine::stop() { is_running_ = false; }

void RealtimeEngine::register_option_token(const std::string &token,
                                           double strike, char type) {
  token_map_[token] = {false, strike, type};
}

void RealtimeEngine::on_tick(const std::string &tick_json) {
  if (!is_running_)
    return;

  try {
    // We pad the string to satisfy simdjson requirements
    simdjson::padded_string padded(tick_json);
    simdjson::ondemand::document doc = parser_.iterate(padded);

    std::string_view t_val;
    if (doc["t"].get(t_val) != simdjson::SUCCESS)
      return; // not a tick

    std::string_view tk_val;
    std::string_view e_val;

    if (doc["tk"].get(tk_val) != simdjson::SUCCESS)
      return;

    // e_val can be missing if only tk is passed sometimes, but usually e and tk
    // are present.
    std::string token_str;
    if (doc["e"].get(e_val) == simdjson::SUCCESS) {
      token_str = std::string(e_val) + "|" + std::string(tk_val);
    } else {
      token_str = std::string(tk_val); // fallback
    }

    // find in map
    auto it = token_map_.find(token_str);
    if (it == token_map_.end()) {
      // Shoonya might just send tk_val
      it = token_map_.find(std::string(tk_val));
      if (it == token_map_.end())
        return; // Unknown token
      token_str = std::string(tk_val);
    }

    const auto &info = it->second;

    // Parse LTP
    std::string_view lp_val;
    if (doc["lp"].get(lp_val) == simdjson::SUCCESS) {
      double ltp = 0.0;
      // lp_val is string like "25000.50"
      auto result =
          std::from_chars(lp_val.data(), lp_val.data() + lp_val.size(), ltp);
      if (result.ec == std::errc()) {
        md_cache_->update_ltp(token_str, ltp);

        if (info.is_underlying) {
          // Update ATM and Subscriptions
          sub_mgr_->on_underlying_tick(ltp);
          // Update Option Chain Underlying
          chain_mgr_->set_underlying_ltp(ltp);
        } else {
          // Update Option Chain Option
          MarketData md = md_cache_->get_market_data(token_str);
          chain_mgr_->update_option_tick(token_str, info.strike, info.type, md);
        }
      }
    }

  } catch (const simdjson::simdjson_error &e) {
    std::cerr << "[RealtimeEngine] JSON Parse Error: " << e.what() << "\n";
  } catch (...) {
    std::cerr << "[RealtimeEngine] Unknown Error\n";
  }
}

} // namespace nexus::engine
