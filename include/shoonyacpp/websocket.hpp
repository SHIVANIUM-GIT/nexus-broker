#pragma once
#include <functional>
#include <string>
#include <thread>

#include "user.hpp"

namespace shoonyacpp {

class NorenWebsocket {
public:
  explicit NorenWebsocket(const std::string &ws_endpoint,
                          const UserCredentials &credentials);
  ~NorenWebsocket();

  void connect();
  void disconnect();
  void subscribe(const std::string &instrument);
  void unsubscribe(const std::string &instrument);

  void set_on_tick_callback(
      std::function<void(const std::string &tick_json)> callback);
private:
  std::string ws_endpoint_;
  UserCredentials credentials_;
  void *ws_session_;
  std::thread background_thread_;
  bool is_running_;

  std::function<void(const std::string &)> on_tick_callback_;
};
} // namespace shoonyacpp   
