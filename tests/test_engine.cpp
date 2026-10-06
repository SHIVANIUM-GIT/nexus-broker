#include <cassert>
#include <iostream>
#include <cmath>
#include <set>
#include "engine/SubscriptionManager.hpp"
#include "greek/Greek.hpp"

using namespace nexus::engine;
using namespace nexus::greek;

class MockSubClient : public ISubscriptionClient {
public:
    std::set<std::string> subbed;

    void subscribe(const std::string& inst) override {
        subbed.insert(inst);
    }
    void unsubscribe(const std::string& inst) override {
        subbed.erase(inst);
    }
};

void test_atm() {
    MockSubClient client;
    SubscriptionManager mgr(&client, 50, 5, 10);
    assert(mgr.calculate_atm(25000.0) == 25000);
    assert(mgr.calculate_atm(25001.0) == 25000);
    assert(mgr.calculate_atm(25024.0) == 25000);
    assert(mgr.calculate_atm(25026.0) == 25050);
    assert(mgr.calculate_atm(24976.0) == 25000);
    assert(mgr.calculate_atm(24974.0) == 24950);
    std::cout << "test_atm passed.\n";
}

void test_subscriptions() {
    MockSubClient client;
    SubscriptionManager mgr(&client, 50, 5, 10);
    
    mgr.set_instrument_mapper([](double strike, const std::string& type) {
        return std::to_string(static_cast<int>(strike)) + type;
    });

    mgr.on_underlying_tick(25000.0); // ATM 25000
    // Buffer is 10, so ATM-10 to ATM+10 = 21 strikes * 2 types = 42 contracts
    assert(client.subbed.size() == 42);
    assert(client.subbed.find("25000CE") != client.subbed.end());
    assert(client.subbed.find("24500CE") != client.subbed.end());
    assert(client.subbed.find("25500PE") != client.subbed.end());
    
    // Move slightly, ATM doesn't change
    mgr.on_underlying_tick(25010.0);
    assert(client.subbed.size() == 42);

    // Move to next strike
    mgr.on_underlying_tick(25026.0); // ATM 25050
    assert(client.subbed.size() == 42);
    assert(client.subbed.find("24500CE") == client.subbed.end()); // dropped
    assert(client.subbed.find("25550CE") != client.subbed.end()); // added
    
    std::cout << "test_subscriptions passed.\n";
}

void test_iv() {
    double S = 25000.0;
    double K = 25000.0;
    double T = 30.0 / 365.0;
    double r = 0.05;
    
    // Target IV = 15%
    double expected_price = bs_price('C', r, S, K, T, 0.15);
    
    double iv = calculate_iv('C', S, K, T, r, expected_price);
    assert(std::abs(iv - 0.15) < 1e-4);
    
    // Intrinsic value violation
    double iv2 = calculate_iv('C', S, 24000.0, T, r, 900.0); // intrinsic is 1000
    assert(iv2 == 0.0); // Should handle violation

    // Invalid data
    assert(calculate_iv('C', S, K, -1.0, r, 500) == 0.0);
    assert(calculate_iv('C', S, K, T, r, 0) == 0.0);
    
    std::cout << "test_iv passed.\n";
}

int main() {
    test_atm();
    test_subscriptions();
    test_iv();
    std::cout << "All tests passed!\n";
    return 0;
}
