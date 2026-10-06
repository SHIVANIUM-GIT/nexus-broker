#pragma once
#include "greek/Greek.hpp"
#include "engine/MarketDataCache.hpp"
#include <map>
#include <shared_mutex>
#include <mutex>
#include <string>

namespace nexus::engine {

struct OptionData {
    std::string token;
    double strike;
    char type; // 'C' or 'P'
    MarketData market_data;
    nexus::greek::OptionGreeks greeks;
};

class OptionChainManager {
public:
    OptionChainManager(double time_to_expiry, double risk_free_rate)
        : tte_(time_to_expiry), r_(risk_free_rate), underlying_ltp_(0.0) {}

    void set_underlying_ltp(double spot) {
        std::unique_lock lock(mutex_);
        underlying_ltp_ = spot;
        // Optional: Trigger full chain recalculation here if desired.
    }

    void update_option_tick(const std::string& token, double strike, char type, const MarketData& md) {
        std::unique_lock lock(mutex_);
        auto& opt = chain_[strike][type];
        opt.token = token;
        opt.strike = strike;
        opt.type = type;
        opt.market_data = md;

        if (underlying_ltp_ > 0.0 && md.ltp > 0.0 && tte_ > 0.0) {
            double iv = nexus::greek::calculate_iv(type, underlying_ltp_, strike, tte_, r_, md.ltp);

            // Fallback to the opposite option's IV if this one failed (Put-Call Parity)
            if (iv <= 0.00001) {
                char opp_type = (type == 'C') ? 'P' : 'C';
                if (chain_[strike].find(opp_type) != chain_[strike].end()) {
                    double opp_iv = chain_[strike][opp_type].greeks.iv;
                    if (opp_iv > 0.00001) {
                        iv = opp_iv;
                    }
                }
            }

            opt.greeks.iv = iv;
            opt.greeks.delta = nexus::greek::bs_delta(type, underlying_ltp_, strike, tte_, r_, iv);
            opt.greeks.gamma = nexus::greek::bs_gamma(underlying_ltp_, strike, tte_, r_, iv);
            opt.greeks.theta = nexus::greek::bs_theta(type, underlying_ltp_, strike, tte_, r_, iv);
            opt.greeks.vega = nexus::greek::bs_vega(underlying_ltp_, strike, tte_, r_, iv);
            opt.greeks.rho = nexus::greek::bs_rho(type, underlying_ltp_, strike, tte_, r_, iv);

            // Also update the opposite side if it's currently 0 and we now have a valid IV
            if (iv > 0.00001) {
                char opp_type = (type == 'C') ? 'P' : 'C';
                if (chain_[strike].find(opp_type) != chain_[strike].end()) {
                    auto& opp_opt = chain_[strike][opp_type];
                    if (opp_opt.greeks.iv <= 0.00001 && opp_opt.market_data.ltp > 0.0) {
                        opp_opt.greeks.iv = iv;
                        opp_opt.greeks.delta = nexus::greek::bs_delta(opp_type, underlying_ltp_, strike, tte_, r_, iv);
                        opp_opt.greeks.gamma = nexus::greek::bs_gamma(underlying_ltp_, strike, tte_, r_, iv);
                        opp_opt.greeks.theta = nexus::greek::bs_theta(opp_type, underlying_ltp_, strike, tte_, r_, iv);
                        opp_opt.greeks.vega = nexus::greek::bs_vega(underlying_ltp_, strike, tte_, r_, iv);
                        opp_opt.greeks.rho = nexus::greek::bs_rho(opp_type, underlying_ltp_, strike, tte_, r_, iv);
                    }
                }
            }
        }
    }

    OptionData get_option(double strike, char type) const {
        std::shared_lock lock(mutex_);
        auto it = chain_.find(strike);
        if (it != chain_.end()) {
            auto it2 = it->second.find(type);
            if (it2 != it->second.end()) {
                return it2->second;
            }
        }
        return OptionData{};
    }

    std::vector<OptionData> get_all_options() const {
        std::shared_lock lock(mutex_);
        std::vector<OptionData> result;
        for (const auto& [strike, type_map] : chain_) {
            for (const auto& [type, opt] : type_map) {
                result.push_back(opt);
            }
        }
        return result;
    }
    
    double get_underlying_ltp() const {
        std::shared_lock lock(mutex_);
        return underlying_ltp_;
    }

private:
    mutable std::shared_mutex mutex_;
    double tte_;
    double r_;
    double underlying_ltp_;
    
    // strike -> (type -> OptionData)
    std::map<double, std::map<char, OptionData>> chain_;
};

} // namespace nexus::engine
