#include "shoonyacpp/shoonya.hpp"
#include "shoonyacpp/api.hpp"
#include <iostream>

using namespace shoonyacpp;

// Expose post_request for our test (it's private in api.hpp, but we can just use cpr directly or we can make a dummy inherited class)
class TestApi : public NorenRestApi {
public:
    TestApi(const std::string& host) : NorenRestApi(host) {}
    
    void test_user_details() {
        std::string uid = get_credentials().user_id;
        std::string jData = "jData={\"uid\":\"" + uid + "\"}";
        std::string response;
        bool ok = NorenRestApi::post_request("/UserDetails", jData, &response);
        if (ok) {
            std::cout << "\n[SUCCESS] Token is VALID!\n";
            std::cout << "[USER DETAILS] " << response << "\n";
        } else {
            std::cout << "\n[FAILED] Token is INVALID or expired!\n";
            std::cout << "[RESPONSE] " << response << "\n";
        }
    }
};

int main() {
    std::cout << "Checking Access Token...\n";
    try {
        load_dotenv(".env");
    } catch (...) {}

    std::string user_id = get_env_var("USER_ID");
    std::string access_token = get_env_var("ACCESS_TOKEN");

    if (access_token.empty()) {
        std::cerr << "Error: No ACCESS_TOKEN found in .env\n";
        return 1;
    }

    TestApi api("https://api.shoonya.com/NorenWClientAPI");
    
    // We append the auth to the jData. Wait, in Shoonya the authorization is:
    // "jKey=" + access_token. But wait, `NorenRestApi::set_session` actually sets:
    // Header {"Authorization", "Bearer " + access_token} 
    // Let's verify how Shoonya wants the token. Usually they want it in the Authorization header.
    api.set_session(user_id, access_token);
    
    api.test_user_details();

    return 0;
}
