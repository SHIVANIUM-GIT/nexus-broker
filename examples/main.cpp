#include "oms/OrderManager.hpp"
#include "shoonyacpp/aliases.hpp"
#include "shoonyacpp/shoonya.hpp"
#include "shoonyacpp/websocket.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "greek/Greek.hpp"
#include <iomanip>
#include <map>

using namespace shoonyacpp;
using namespace nexus::greek;

double calculate_time_to_expiry(int expiry_year, int expiry_month, int expiry_day) {
    auto now = std::chrono::system_clock::now();
    std::tm expiry_tm = {};
    expiry_tm.tm_year = expiry_year - 1900;
    expiry_tm.tm_mon  = expiry_month - 1;
    expiry_tm.tm_mday = expiry_day;
    expiry_tm.tm_hour = 15;
    expiry_tm.tm_min  = 30;
    std::time_t expiry_time = std::mktime(&expiry_tm);
    auto expiry_tp = std::chrono::system_clock::from_time_t(expiry_time);
    std::chrono::duration<double> diff_in_seconds = expiry_tp - now;
    double days_to_expiry = diff_in_seconds.count() / (60.0 * 60.0 * 24.0);
    return std::max(days_to_expiry / 365.0, 0.0001); // Prevent 0 or negative DTE
}

void print_chain(const OptionChain &chain) {
  std::cout << "\n--- LIVE OPTION CHAIN (Underlying: " << chain.underlying_price
            << ") ---\n";
  std::cout << std::fixed << std::setprecision(4);
  for (const auto &row : chain.rows) {
    std::cout << "STRIKE: " << row.strike << "\n";
    std::cout << "  CALL [Price: " << row.call.price
              << " | IV: " << row.call.greeks.iv * 100
              << "% | Delta: " << row.call.greeks.delta
              << " | Gamma: " << row.call.greeks.gamma
              << " | Theta: " << row.call.greeks.theta
              << " | Vega: " << row.call.greeks.vega << "]\n";
    std::cout << "  PUT  [Price: " << row.put.price
              << " | IV: " << row.put.greeks.iv * 100
              << "% | Delta: " << row.put.greeks.delta
              << " | Gamma: " << row.put.greeks.gamma
              << " | Theta: " << row.put.greeks.theta
              << " | Vega: " << row.put.greeks.vega << "]\n";
  }
  std::cout << "--------------------------------------------------\n";
}

int main() {
  std::cout << "Starting Nexus Broker Engine (with Live Greeks)...\n";

  try {
    load_dotenv(".env");
  } catch (const std::exception &e) {
    std::cout << "[INFO] " << e.what() << " (using fallback values)\n";
  }

  std::string user_id = get_env_var("USER_ID");
  std::string access_token = get_env_var("ACCESS_TOKEN");
  std::string secret_key = get_env_var("SECRET_CODE", get_env_var("API_KEY"));
  std::string auth_code = get_env_var("AUTH_CODE");

  NorenRestApi api("https://api.shoonya.com/NorenWClientAPI");

  if (!access_token.empty()) {
    std::cout << "[INFO] Using existing ACCESS_TOKEN from .env for user: "
              << user_id << '\n';
    api.set_session(user_id, access_token);
  } else if (!auth_code.empty()) {
    bool logged_in =
        api.get_access_token(auth_code, secret_key, user_id + "_U", user_id);
    if (!logged_in) {
      std::cerr << "CRITICAL: Failed to login with AUTH_CODE!\n";
      return 1;
    }
  } else {
    std::cerr << "CRITICAL: No ACCESS_TOKEN or AUTH_CODE found in .env!\n";
    return 1;
  }

  NorenWebsocket ws("wss://api.shoonya.com/NorenWSAPI/", api.get_credentials());

  // Set up the OptionChain for expiry dynamically (e.g., Oct 26, 2026)
  double time_to_expiry = calculate_time_to_expiry(2026, 10, 26);
  double risk_free_rate = 0.1;

  ws.set_on_tick_callback(
      [time_to_expiry, risk_free_rate](const std::string &tick) {
        std::cout << "\n[LIVE DATA] " << tick << std::endl;
        // Parse the live websocket tick for "lp" (last price)
        // Shoonya JSON tick example:
        // {"t":"tf","e":"NSE","tk":"26000","lp":"24000.50"}
        auto pos = tick.find("\"lp\":\"");
        if (pos == std::string::npos) {
          // Also check if it's sent as a raw float
          pos = tick.find("\"lp\":");
          if (pos != std::string::npos)
            pos += 5; // length of "lp":
        } else {
          pos += 6; // length of "lp":"
        }

        if (pos != std::string::npos) {
          auto end = tick.find_first_of(",\"}", pos);
          if (end != std::string::npos) {
            try {
              double underlying_lp = std::stod(tick.substr(pos, end - pos));

              // Create a dynamic OptionChain with the LIVE underlying price
              OptionChain chain(underlying_lp, time_to_expiry, risk_free_rate);

              // Add some arbitrary strikes around the live underlying price
              // Normally, you would fetch these market prices dynamically as
              // well
              double strike1 = std::round(underlying_lp / 100.0) * 100.0;
              double strike2 = strike1 + 100.0;
              print_chain(chain);

            } catch (...) {
              // Ignore parsing errors for live ticks
            }
          }
        }
      });

  ws.connect();
  // Subscribe to NIFTY 50 Index for the underlying live feed
  ws.subscribe("NSE|26000");

  std::cout << "Listening to Websocket Ticks for NSE|26000...\n";
  std::this_thread::sleep_for(std::chrono::hours(6));

  ws.disconnect();
  return 0;
}
