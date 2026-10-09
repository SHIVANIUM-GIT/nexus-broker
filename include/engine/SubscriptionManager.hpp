#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <functional>
#include <cmath>
#include <mutex>
#include <iostream>

namespace nexus::engine {

class ISubscriptionClient {
public:
    virtual ~ISubscriptionClient() = default;
    virtual void subscribe(const std::string& instrument) = 0;
    virtual void unsubscribe(const std::string& instrument) = 0;
};

class SubscriptionManager {
public:
    SubscriptionManager(ISubscriptionClient* client, int strike_interval, int subscribe_depth, int buffer_depth)
        : client_(client), strike_interval_(strike_interval), subscribe_depth_(subscribe_depth), buffer_depth_(buffer_depth), current_atm_(0) {}

    // Maps strike and option type to instrument token. e.g. (25000, "CE") -> "NFO|12345"
    using InstrumentMapper = std::function<std::string(double strike, const std::string& type)>;
    void set_instrument_mapper(InstrumentMapper mapper) { mapper_ = mapper; }

    int calculate_atm(double spot) {
        return static_cast<int>(std::round(spot / strike_interval_) * strike_interval_);
    }

    void on_underlying_tick(double spot) {
        if (!mapper_) return;

        int new_atm = calculate_atm(spot);
        if (new_atm == current_atm_) {
            return; // No change in ATM
        }

        std::lock_guard<std::mutex> lock(mutex_);
        
        // Strategy required strikes (e.g., ATM ± 5)
        std::unordered_set<std::string> required_instruments;
        for (int i = -buffer_depth_; i <= buffer_depth_; ++i) {
            double strike = new_atm + (i * strike_interval_);
            required_instruments.insert(mapper_(strike, "CE"));
            required_instruments.insert(mapper_(strike, "PE"));
        }

        // Unsubscribe from obsolete contracts
        std::string unsub_batch;
        std::vector<std::string> to_unsubscribe;
        for (const auto& inst : active_subscriptions_) {
            if (required_instruments.find(inst) == required_instruments.end()) {
                to_unsubscribe.push_back(inst);
                if (!unsub_batch.empty()) unsub_batch += "#";
                unsub_batch += inst;
            }
        }

        if (!unsub_batch.empty()) {
            client_->unsubscribe(unsub_batch);
        }
        for (const auto& inst : to_unsubscribe) {
            active_subscriptions_.erase(inst);
        }

        // Subscribe to new contracts
        std::string sub_batch;
        for (const auto& inst : required_instruments) {
            if (active_subscriptions_.find(inst) == active_subscriptions_.end()) {
                if (!sub_batch.empty()) sub_batch += "#";
                sub_batch += inst;
                active_subscriptions_.insert(inst);
            }
        }
        
        if (!sub_batch.empty()) {
            client_->subscribe(sub_batch);
        }

        current_atm_ = new_atm;
        // std::cout << "[SubMgr] ATM Updated to " << current_atm_ << "\n";
    }

    int get_current_atm() const { return current_atm_; }

private:
    ISubscriptionClient* client_;
    int strike_interval_;
    int subscribe_depth_;
    int buffer_depth_;
    int current_atm_;
    InstrumentMapper mapper_;
    std::unordered_set<std::string> active_subscriptions_;
    std::mutex mutex_;
};

} // namespace nexus::engine
