#pragma once    
#include <string>

namespace shoonyacpp
{

struct UserCredentials
{
    std::string user_id;
    std::string account_id;
    std::string password;
    std::string suser_token;
    std::string access_token;

    [[nodiscard]] bool is_logged_in() const
    {
        return !user_id.empty() && !suser_token.empty();
    }

    void clear()
    {
        user_id.clear();
        account_id.clear();
        password.clear();
        suser_token.clear();
        access_token.clear();
    }
};

}