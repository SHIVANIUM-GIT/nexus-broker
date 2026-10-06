#pragma once
#include <string>
#include <vector>

#include "Optionchain.hpp"
#include "Order.hpp"
#include "OrderBook.hpp"
#include "Position.hpp"
#include "Quote.hpp"
#include "TradeBook.hpp"
#include "user.hpp"

namespace shoonyacpp {

class NorenRestApi {
public:
  explicit NorenRestApi(const std::string &host);
  ~NorenRestApi();

  NorenRestApi(const NorenRestApi &) = delete;
  NorenRestApi &operator=(const NorenRestApi &) = delete;

  NorenRestApi(NorenRestApi &&) noexcept;
  NorenRestApi &operator=(NorenRestApi &&) noexcept;

  std::string get_oauth_url(const std::string &oauth_url,
                            const std::string &api_key);

  bool get_access_token(const std::string &authcode,
                        const std::string &secret_key,
                        const std::string &user_id, const std::string &uid);

  bool get_session(const std::string &user_id, const std::string &password,
                   const std::string &user_token,
                   const std::string &access_token);

  void set_session(const std::string &user_id, const std::string &access_token);

  bool place_order(const Order &order);
  bool modify_order(const std::string &order_no, const Order &new_order);
  bool cancel_order(const std::string &order_no);

  std::vector<OrderBook> get_order_book();
  std::vector<TradeBook> get_trade_book();
  std::vector<Position> get_positions();
  std::vector<OptionChainItem> get_option_chain(const std::string &exchange,
                                                const std::string &symbol,
                                                double center_strike,
                                                int count = 10);
  Quote get_quotes(const std::string &exchange, const std::string &token);

  const UserCredentials &get_credentials() const { return credentials_; }

private:
  std::string host_url_;
  UserCredentials credentials_;

  void *http_session_;

public:
  bool post_request(const std::string &route, const std::string &jData,
                    std::string *out_response = nullptr);
};

} // namespace shoonyacpp
