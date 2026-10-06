#include "engine/RealtimeEngine.hpp"
#include <chrono>
#include <iostream>
#include <vector>

using namespace nexus::engine;
using namespace shoonyacpp;

int main() {
    UserCredentials creds{"USER", "TOKEN"};
    NorenWebsocket ws("wss://dummy", creds);
    RealtimeEngine engine(ws, "NSE|26000", 50);
    engine.start(30.0 / 365.0, 0.05);

    // Mock tick
    std::string tick_json = "{\"t\":\"tf\",\"e\":\"NSE\",\"tk\":\"26000\",\"lp\":\"25000.50\"}";

    int num_iterations = 1000000;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; ++i) {
        engine.on_tick(tick_json);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    std::cout << "Processed " << num_iterations << " ticks in " << duration.count() << " us.\n";
    std::cout << "Latency per tick: " << static_cast<double>(duration.count()) / num_iterations << " us.\n";

    return 0;
}
