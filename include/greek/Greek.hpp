#pragma once

#include <cmath>
#include <memory>
#include <numbers>
#include <vector>
#include <iostream>

namespace nexus::greek {

constexpr double r = 0.07;

[[nodiscard]] inline double norm_cdf(double x) {
  return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

[[nodiscard]] inline double norm_pdf(double x) {
  return (1.0 / std::sqrt(2.0 * std::numbers::pi)) * std::exp(-0.5 * x * x);
}

[[nodiscard]] inline double bs_price(char type, double r, double S, double K,
                                     double T, double V) {
  if (T <= 0.0 || V <= 0.0) {
      if (type == 'C') return std::max(0.0, S - K);
      else return std::max(0.0, K - S);
  }

  double d1 = (std::log(S / K) + (r + V * V / 2.0) * T) / (V * std::sqrt(T));
  double d2 = d1 - V * std::sqrt(T);

  if (type == 'C') {
    return S * norm_cdf(d1) - K * std::exp(-r * T) * norm_cdf(d2);
  } else {
    return K * std::exp(-r * T) * norm_cdf(-d2) - S * norm_cdf(-d1);
  }
}

[[nodiscard]] inline double bs_vega(double S, double K, double T, double r,
                                    double V) {
  if (T <= 0.0 || V <= 0.0)
    return 0.0;
  double d1 = (std::log(S / K) + (r + V * V / 2.0) * T) / (V * std::sqrt(T));
  return S * norm_pdf(d1) * std::sqrt(T);
}

[[nodiscard]] inline double bs_delta(char type, double S, double K, double T,
                                     double r, double V) {
  if (T <= 0.0 || V <= 0.0)
    return (type == 'C') ? (S > K ? 1.0 : 0.0) : (S < K ? -1.0 : 0.0);
  double d1 = (std::log(S / K) + (r + V * V / 2.0) * T) / (V * std::sqrt(T));
  if (type == 'C') {
    return norm_cdf(d1);
  } else {
    return norm_cdf(d1) - 1.0;
  }
}

[[nodiscard]] inline double bs_gamma(double S, double K, double T, double r,
                                     double V) {
  if (T <= 0.0 || V <= 0.0)
    return 0.0;
  double d1 = (std::log(S / K) + (r + V * V / 2.0) * T) / (V * std::sqrt(T));
  return norm_pdf(d1) / (S * V * std::sqrt(T));
}

[[nodiscard]] inline double bs_theta(char type, double S, double K, double T,
                                     double r, double V) {
  if (T <= 0.0 || V <= 0.0)
    return 0.0;
  double d1 = (std::log(S / K) + (r + V * V / 2.0) * T) / (V * std::sqrt(T));
  double d2 = d1 - V * std::sqrt(T);

  double term1 = -(S * norm_pdf(d1) * V) / (2.0 * std::sqrt(T));
  if (type == 'C') {
    double term2 = r * K * std::exp(-r * T) * norm_cdf(d2);
    return term1 - term2;
  } else {
    double term2 = r * K * std::exp(-r * T) * norm_cdf(-d2);
    return term1 + term2;
  }
}

[[nodiscard]] inline double bs_rho(char type, double S, double K, double T,
                                   double r, double V) {
  if (T <= 0.0 || V <= 0.0)
    return 0.0;
  double d1 = (std::log(S / K) + (r + V * V / 2.0) * T) / (V * std::sqrt(T));
  double d2 = d1 - V * std::sqrt(T);
  if (type == 'C') {
    return K * T * std::exp(-r * T) * norm_cdf(d2);
  } else {
    return -K * T * std::exp(-r * T) * norm_cdf(-d2);
  }
}

[[nodiscard]] inline double calculate_iv(char type, double S, double K,
                                         double T, double r, double P_market) {
  if (T <= 0.0 || P_market <= 0.0)
    return 0.0;

  // Intrinsic value check
  double intrinsic = (type == 'C') ? std::max(0.0, S - K) : std::max(0.0, K - S);
  if (P_market < intrinsic) {
      return 0.0; // Violation of lower bound
  }

  double sigma = 0.5;
  const double max_iter = 100;
  const double tol = 1e-5;
  
  // Try Newton-Raphson first
  for (int i = 0; i < max_iter; ++i) {
    double price = bs_price(type, r, S, K, T, sigma);
    double vega = bs_vega(S, K, T, r, sigma);

    if (vega < 1e-6)
      break;

    double diff = P_market - price;
    if (std::abs(diff) < tol) {
      return sigma;
    }
    sigma += diff / vega;
    if (sigma <= 0.0) {
        break; // Fallback to bisection
    }
  }

  // Bisection fallback
  double low = 1e-5;
  double high = 5.0; // 500% IV max
  
  if (bs_price(type, r, S, K, T, high) < P_market) {
      return high; // Too high
  }

  for (int i = 0; i < max_iter; ++i) {
      sigma = (low + high) / 2.0;
      double price = bs_price(type, r, S, K, T, sigma);
      if (std::abs(price - P_market) < tol) {
          return sigma;
      }
      if (price < P_market) {
          low = sigma;
      } else {
          high = sigma;
      }
  }

  return sigma;
}

struct OptionGreeks {
  double iv;
  double delta;
  double gamma;
  double theta;
  double vega;
  double rho;
};

struct OptionContract {
  char type;
  double strike;
  double price;
  OptionGreeks greeks;
};

struct OptionChainRow {
  double strike;
  OptionContract call;
  OptionContract put;
};

class OptionChain {
public:
  std::vector<OptionChainRow> rows;
  double underlying_price;
  double time_to_expiry;
  double risk_free_rate;

  OptionChain(double S, double T, double r)
      : underlying_price(S), time_to_expiry(T), risk_free_rate(r) {}

  void add_strike(double strike, double call_price, double put_price) {
    OptionChainRow row;
    row.strike = strike;

    // Call
    row.call.type = 'C';
    row.call.strike = strike;
    row.call.price = call_price;
    double call_iv = calculate_iv('C', underlying_price, strike, time_to_expiry,
                                  risk_free_rate, call_price);
    row.call.greeks = {
        call_iv,
        bs_delta('C', underlying_price, strike, time_to_expiry, risk_free_rate, call_iv),
        bs_gamma(underlying_price, strike, time_to_expiry, risk_free_rate, call_iv),
        bs_theta('C', underlying_price, strike, time_to_expiry, risk_free_rate, call_iv),
        bs_vega(underlying_price, strike, time_to_expiry, risk_free_rate, call_iv),
        bs_rho('C', underlying_price, strike, time_to_expiry, risk_free_rate, call_iv)};

    // Put
    row.put.type = 'P';
    row.put.strike = strike;
    row.put.price = put_price;
    double put_iv = calculate_iv('P', underlying_price, strike, time_to_expiry,
                                 risk_free_rate, put_price);
    row.put.greeks = {
        put_iv,
        bs_delta('P', underlying_price, strike, time_to_expiry, risk_free_rate, put_iv),
        bs_gamma(underlying_price, strike, time_to_expiry, risk_free_rate, put_iv),
        bs_theta('P', underlying_price, strike, time_to_expiry, risk_free_rate, put_iv),
        bs_vega(underlying_price, strike, time_to_expiry, risk_free_rate, put_iv),
        bs_rho('P', underlying_price, strike, time_to_expiry, risk_free_rate, put_iv)};

    rows.push_back(row);
  }
};

} // namespace nexus::greek
