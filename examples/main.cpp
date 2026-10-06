#include "oms/OrderManager.hpp"
#include "shoonyacpp/aliases.hpp"
#include "shoonyacpp/shoonya.hpp"
#include "shoonyacpp/websocket.hpp"
#include "engine/RealtimeEngine.hpp"
#include "engine/OptionSymbolManager.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <map>

using namespace shoonyacpp;
using namespace nexus::engine;

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

void print_realtime_chain(OptionChainManager& chain_mgr) {
    std::cout << "\n--- LIVE REALTIME OPTION CHAIN (Underlying: " << chain_mgr.get_underlying_ltp() << ") ---\n";
    std::cout << std::fixed << std::setprecision(4);
    auto all_options = chain_mgr.get_all_options();
    
    // Group by strike
    std::map<double, std::pair<OptionData, OptionData>> grouped;
    for (const auto& opt : all_options) {
        if (opt.type == 'C') grouped[opt.strike].first = opt;
        else grouped[opt.strike].second = opt;
    }

    for (const auto& [strike, pair] : grouped) {
        const auto& call = pair.first;
        const auto& put = pair.second;
        
        std::cout << "STRIKE: " << strike << "\n";
        std::cout << "  CALL [Price: " << call.market_data.ltp
                  << " | IV: " << call.greeks.iv * 100
                  << "| Delta: " << call.greeks.delta
                  << " | Gamma: " << call.greeks.gamma
                  << " | Theta: " << call.greeks.theta
                  << " | Vega: " << call.greeks.vega << "]\n";
        std::cout << "  PUT  [Price: " << put.market_data.ltp
                  << " | IV: " << put.greeks.iv * 100
                  << "% | Delta: " << put.greeks.delta
                  << " | Gamma: " << put.greeks.gamma
                  << " | Theta: " << put.greeks.theta
                  << " | Vega: " << put.greeks.vega << "]\n";
    }
    std::cout << "--------------------------------------------------\n";
}

int main() {
  std::cout << "Starting Nexus Broker Realtime Greeks Engine...\n";

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
    std::cout << "[INFO] Using existing ACCESS_TOKEN from .env for user: " << user_id << '\n';
    api.set_session(user_id, access_token);
  } else if (!auth_code.empty()) {
    bool logged_in = api.get_access_token(auth_code, secret_key, user_id + "_U", user_id);
    if (!logged_in) {
      std::cerr << "CRITICAL: Failed to login with AUTH_CODE!\n";
      return 1;
    }
  } else {
    std::cerr << "CRITICAL: No ACCESS_TOKEN or AUTH_CODE found in .env!\n";
    return 1;
  }

  // Verify token before starting WebSocket
  std::string response;
  std::string jData = "jData={\"uid\":\"" + api.get_credentials().user_id + "\"}";
  if (!api.post_request("/UserDetails", jData, &response)) {
      std::cerr << "CRITICAL: Access token is invalid or expired! Cannot start WebSocket.\n";
      return 1;
  }
  std::cout << "[INFO] Token verified successfully.\n";

  NorenWebsocket ws("wss://api.shoonya.com/NorenWSAPI/", api.get_credentials());

  // Set up the Realtime Engine for NIFTY 50 Index
  // Underlying Token: NSE|26000 (Nifty 50), Strike interval: 50
  RealtimeEngine engine(ws, "NSE|26000", 50);

  // Load Options Symbol Master Dynamically
  OptionSymbolManager sym_mgr("NIFTY");
  if (!sym_mgr.initialize()) {
      std::cerr << "CRITICAL: Failed to initialize option symbols. Exiting...\n";
      return 1;
  }

  engine.set_instrument_mapper([&sym_mgr](double strike, const std::string& type) -> std::string {
      return sym_mgr.get_token(strike, type[0]);
  });

  auto [exp_y, exp_m, exp_d] = sym_mgr.get_nearest_expiry_date();
  double time_to_expiry = calculate_time_to_expiry(exp_y, exp_m, exp_d);
  double risk_free_rate = 0.05;

  ws.connect();
  std::this_thread::sleep_for(std::chrono::seconds(2)); // wait for connection to establish

  engine.start(time_to_expiry, risk_free_rate);

  std::cout << "Listening to Websocket Ticks for NSE|26000 and Options...\n";
  
  // A simple background thread to print chain every 1 second for observation
  std::thread printer([&engine]() {
      while(true) {
          std::this_thread::sleep_for(std::chrono::milliseconds(500)); // MUST have sleep to prevent CPU/console crash!
          print_realtime_chain(engine.get_chain_manager());
      }
  });

  std::this_thread::sleep_for(std::chrono::hours(6));

  engine.stop();
  ws.disconnect();
  
  printer.detach();
  return 0;
}
