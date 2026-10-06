#pragma once
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <cstdint>
#include <vector>

namespace nexus::engine {

struct MarketData {
    double ltp = 0.0;
    double bid = 0.0;
    double ask = 0.0;
    int64_t volume = 0;
    uint64_t timestamp = 0;
};

class MarketDataCache {
public:
    void update_tick(const std::string& instrument, const MarketData& data) {
        std::unique_lock lock(mutex_);
        cache_[instrument] = data;
    }

    void update_ltp(const std::string& instrument, double ltp) {
        std::unique_lock lock(mutex_);
        cache_[instrument].ltp = ltp;
    }

    MarketData get_market_data(const std::string& instrument) const {
        std::shared_lock lock(mutex_);
        auto it = cache_.find(instrument);
        if (it != cache_.end()) {
            return it->second;
        }
        return MarketData{};
    }

    double get_ltp(const std::string& instrument) const {
        std::shared_lock lock(mutex_);
        auto it = cache_.find(instrument);
        if (it != cache_.end()) {
            return it->second.ltp;
        }
        return 0.0;
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, MarketData> cache_;
};

} // namespace nexus::engine
