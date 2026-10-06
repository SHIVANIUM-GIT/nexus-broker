#include "engine/RealtimeEngine.hpp"
#include "shoonyacpp/shoonya.hpp"
#include "shoonyacpp/websocket.hpp"
#include <chrono>
#include <iostream>
#include <vector>
#include <numeric>
#include <thread>
#include <atomic>

using namespace nexus::engine;
using namespace shoonyacpp;

std::atomic<int> tick_count{0};
std::atomic<long long> total_latency_ns{0};

int main() {
    std::cout << "Starting Live Websocket Benchmark...\n";

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

    // Set up Realtime Engine for NIFTY 50 Index (NSE|26000)
    RealtimeEngine engine(ws, "NSE|26000", 50);
    double time_to_expiry = 30.0 / 365.0; // dummy expiry
    engine.start(time_to_expiry, 0.05);

    // Override the websocket callback to measure latency internally
    // Note: The RealtimeEngine also sets a callback in its start() method, 
    // but we will wrap it here for benchmarking.
    ws.set_on_tick_callback([&engine](const std::string& tick) {
        auto start = std::chrono::high_resolution_clock::now();
        
        engine.on_tick(tick);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        total_latency_ns += duration.count();
        int current_count = ++tick_count;

        if (current_count <= 5) {
            std::cout << "[RAW TICK " << current_count << "] " << tick << "\n";
        }
    });

    ws.connect();
    std::cout << "Connecting to WebSocket...\n";
    std::this_thread::sleep_for(std::chrono::seconds(2)); // wait for connection
    
    std::cout << "Subscribing to NSE|26000 and Option...\n";
    ws.subscribe("NSE|26000");
    ws.subscribe("NFO|NIFTY08OCT26C22700"); // Underlying NIFTY

    // Also subscribe to a mock active option to benchmark Greek calculation
    // Since we don't have a real token right now, we can just feed a mock tick, or subscribe to random NFO tokens if known.
    // For this benchmark, we just let it run on whatever ticks arrive (at least the underlying).

    // Background thread to print stats
    std::thread reporter([]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            int count = tick_count.exchange(0);
            long long latency = total_latency_ns.exchange(0);

            if (count > 0) {
                double avg_ns = static_cast<double>(latency) / count;
                std::cout << "[STATS] Ticks in last 5s: " << count 
                          << " | Avg Engine Latency: " << avg_ns << " ns/tick (" 
                          << avg_ns / 1000.0 << " us)\n";
            } else {
                std::cout << "[STATS] No ticks received in last 5s.\n";
            }
        }
    });

    // Run for 30 seconds
    std::this_thread::sleep_for(std::chrono::seconds(30));

    engine.stop();
    ws.disconnect();
    reporter.detach();
    
    std::cout << "Live Benchmark Finished.\n";
    return 0;
}
