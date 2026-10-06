#pragma once

#include "shoonyacpp/websocket.hpp"
#include "engine/MarketDataCache.hpp"
#include "engine/SubscriptionManager.hpp"
#include "engine/OptionChainManager.hpp"
#include <simdjson.h>
#include <thread>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <string_view>
#include <functional>

namespace nexus::engine {

class WsClientWrapper : public ISubscriptionClient {
public:
    WsClientWrapper(shoonyacpp::NorenWebsocket& ws) : ws_(ws) {}
    void subscribe(const std::string& instrument) override {
        ws_.subscribe(instrument);
    }
    void unsubscribe(const std::string& instrument) override {
        ws_.unsubscribe(instrument);
    }
private:
    shoonyacpp::NorenWebsocket& ws_;
};

struct TokenInfo {
    bool is_underlying;
    double strike;
    char type;
};

class RealtimeEngine {
public:
    RealtimeEngine(shoonyacpp::NorenWebsocket& ws, const std::string& underlying_token, int strike_interval);
    ~RealtimeEngine();

    void start(double time_to_expiry, double risk_free_rate = 0.01);
    void stop();

    void on_tick(const std::string& tick_json);

    OptionChainManager& get_chain_manager() { return *chain_mgr_; }
    MarketDataCache& get_market_data_cache() { return *md_cache_; }

    void set_instrument_mapper(std::function<std::string(double, const std::string&)> mapper) {
        custom_mapper_ = std::move(mapper);
    }

    // Helpers to map token
    void register_option_token(const std::string& token, double strike, char type);

private:
    shoonyacpp::NorenWebsocket& ws_;
    std::string underlying_token_;
    int strike_interval_;

    std::unique_ptr<WsClientWrapper> ws_wrapper_;
    std::unique_ptr<MarketDataCache> md_cache_;
    std::unique_ptr<SubscriptionManager> sub_mgr_;
    std::unique_ptr<OptionChainManager> chain_mgr_;

    std::atomic<bool> is_running_;
    
    // simdjson parser per thread (we assume on_tick is called from a single WS thread)
    simdjson::ondemand::parser parser_;

    // Mapping from token to instrument info
    std::unordered_map<std::string, TokenInfo> token_map_;
    
    std::function<std::string(double, const std::string&)> custom_mapper_;
};

} // namespace nexus::engine
