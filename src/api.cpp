#include "shoonyacpp/api.hpp"
#include "cpr/api.h"
#include "cpr/bearer.h"
#include "cpr/body.h"
#include "cpr/callback.h"
#include "cpr/cprtypes.h"
#include "shoonyacpp/Position.hpp"
#include "simdjson.h"
#include <cpr/cpr.h>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>

namespace shoonyacpp {

NorenRestApi::NorenRestApi(const std::string &host) {
  host_url_ = host;
  auto *session = new cpr::Session();
  session->ResponseStringReserve(1024);
  http_session_ = session;
}

NorenRestApi::~NorenRestApi() {
  if (http_session_) {
    delete static_cast<cpr::Session *>(http_session_);
    http_session_ = nullptr;
  }
}

NorenRestApi::NorenRestApi(NorenRestApi &&other) noexcept
    : host_url_(std::move(other.host_url_)),
      credentials_(std::move(other.credentials_)),
      http_session_(other.http_session_) {
  other.http_session_ = nullptr;
}

NorenRestApi &NorenRestApi::operator=(NorenRestApi &&other) noexcept {
  if (this != &other) {
    if (http_session_) {
      delete static_cast<cpr::Session *>(http_session_);
    }
    host_url_ = std::move(other.host_url_);
    credentials_ = std::move(other.credentials_);
    http_session_ = other.http_session_;
    other.http_session_ = nullptr;
  }
  return *this;
}

void NorenRestApi::set_session(const std::string &user_id,
                               const std::string &access_token) {
  credentials_.user_id = user_id;
  credentials_.access_token = access_token;

  if (http_session_) {
    auto *session = static_cast<cpr::Session *>(http_session_);
    session->SetHeader(
        cpr::Header{{"Authorization", "Bearer " + access_token},
                    {"Content-Type", "application/x-www-form-urlencoded"}});
  }
}

std::string NorenRestApi::get_oauth_url(const std::string &oauth_url,
                                        const std::string &api_key) {
  return oauth_url + "?client_id=" + api_key;
}

bool NorenRestApi::get_access_token(const std::string &auth_code,
                                    const std::string &secret_key,
                                    const std::string &user_id,
                                    const std::string &uid) {
  std::string endpoint = host_url_ + "/GenAcsTok";

  std::string json_body;
  json_body.reserve(256);
  json_body += "{\"authcode\":\"";
  json_body += auth_code;
  json_body += "\",\"secret_key\":\"";
  json_body += secret_key;
  json_body += "\",\"client_id\":\"";
  json_body += user_id;
  json_body += "\",\"uid\":\"";
  json_body += uid;
  json_body += "\"}";

  cpr::Response r =
      cpr::Post(cpr::Url{endpoint}, cpr::Body{json_body},
                cpr::Header{{"Content-Type", "application/json"}});

  return (r.status_code == 200);
}

bool NorenRestApi::get_session(const std::string &user_id,
                               const std::string &password,
                               const std::string &user_token,
                               const std::string &access_token) {
  return true;
}

bool NorenRestApi::post_request(const std::string &route,
                                const std::string &jData,
                                std::string *out_response) {
  if (!http_session_)
    return false;

  auto *session = static_cast<cpr::Session *>(http_session_);
  session->SetUrl(cpr::Url{host_url_ + route});
  session->SetBody(cpr::Body{jData});

  cpr::Response r = session->Post();

  bool ok = (r.status_code == 200 &&
             r.text.find("\"stat\":\"Ok\"") != std::string::npos);

  if (ok) {
    std::cout << "[EXCHANGE ACK] " << route << " -> " << r.text << "\n";
  } else {
    std::cerr << "[EXCHANGE REJECT/ERROR] " << route << " -> Status: " << r.status_code << " | " << r.text << "\n";
  }

  if (out_response) {
    *out_response = std::move(r.text);
  }

  return ok;
}

bool NorenRestApi::place_order(const Order &order) {
  char side_char = static_cast<char>(order.side);
  std::string_view prc_type = to_string(order.price_type);
  double price = order.price.value_or(0.0);

  std::string jData;
  jData.reserve(256);
  jData += "jData={\"uid\":\"";
  jData += credentials_.user_id;
  jData += "\",\"actid\":\"";
  jData += credentials_.user_id;
  jData += "\",\"trantype\":\"";
  jData += side_char;
  jData += "\",\"prd\":\"I\",\"exch\":\"";
  jData += order.exchange;
  jData += "\",\"tsym\":\"";
  jData += order.trading_symbol;
  jData += "\",\"qty\":\"";
  jData += std::to_string(order.quantity);
  jData += "\",\"prctyp\":\"";
  jData += prc_type;
  jData += "\",\"prc\":\"";
  jData += std::to_string(price);
  jData += "\",\"ret\":\"DAY\"}";

  return post_request("/PlaceOrder", jData);
}

bool NorenRestApi::modify_order(const std::string &order_no,
                                const Order &new_order) {
  std::string_view prc_type = to_string(new_order.price_type);
  double price = new_order.price.value_or(0.0);

  std::string jData;
  jData.reserve(256);
  jData += "jData={\"uid\":\"";
  jData += credentials_.user_id;
  jData += "\",\"norenordno\":\"";
  jData += order_no;
  jData += "\",\"exch\":\"";
  jData += new_order.exchange;
  jData += "\",\"tsym\":\"";
  jData += new_order.trading_symbol;
  jData += "\",\"qty\":\"";
  jData += std::to_string(new_order.quantity);
  jData += "\",\"prctyp\":\"";
  jData += prc_type;
  jData += "\",\"prc\":\"";
  jData += std::to_string(price);
  jData += "\",\"ret\":\"DAY\"}";

  return post_request("/ModifyOrder", jData);
}

bool NorenRestApi::cancel_order(const std::string &order_no) {
  std::string jData;
  jData.reserve(128);
  jData += "jData={\"uid\":\"";
  jData += credentials_.user_id;
  jData += "\",\"norenordno\":\"";
  jData += order_no;
  jData += "\"}";

  return post_request("/CancelOrder", jData);
}

std::vector<Position> NorenRestApi::get_positions() {
  std::vector<Position> positions;

  std::string jData = "jData={\"uid\":\"" + credentials_.user_id +
                      "\",\"actid\":\"" + credentials_.user_id + "\"}";

  std::string response_text;
  bool ok = post_request("/PositionBook", jData, &response_text);

  if (!ok || response_text.empty()) {
    return positions;
  }

  try {
    simdjson::ondemand::parser parser;
    simdjson::padded_string padded_json(response_text);
    simdjson::ondemand::document doc = parser.iterate(padded_json);

    auto array_result = doc.get_array();
    if (array_result.error()) {

#ifndef NDEBUG
      std::cout
          << "[INFO] No open positions found (response is not an array).\n";
#endif

      return positions;
    }

    for (simdjson::ondemand::object item : array_result) {
      Position pos{};

      auto prd = item["prd"];
      if (!prd.error())
        pos.prd = std::string(std::string_view(prd.value()));

      auto exch = item["exch"];
      if (!exch.error())
        pos.exch = std::string(std::string_view(exch.value()));

      auto tsym = item["tsym"];
      if (!tsym.error())
        pos.symname = std::string(std::string_view(tsym.value()));

      auto netqty = item["netqty"];
      if (!netqty.error()) {
        std::string_view sv = netqty.value();
        pos.netqty = std::stoi(std::string(sv));
      }

      auto buyqty = item["daybuyqty"];
      if (!buyqty.error()) {
        std::string_view sv = buyqty.value();
        pos.buyqty = std::stoi(std::string(sv));
      }

      auto sellqty = item["daysellqty"];
      if (!sellqty.error()) {
        std::string_view sv = sellqty.value();
        pos.sellqty = std::stoi(std::string(sv));
      }

      positions.push_back(pos);
    }
  } catch (const simdjson::simdjson_error &e) {
    std::cerr << "[SIMDJSON ERROR] " << e.what() << '\n';
  }

  return positions;
}

template <typename T>
void get_field(simdjson::ondemand::object &item, const char *key, T &dest) {
  auto val = item[key];
  if (!val.error()) {
    try {
      if constexpr (std::is_same_v<T, std::string>) {
        dest = std::string(std::string_view(val.value()));
      } else if constexpr (std::is_same_v<T, int32_t> ||
                           std::is_same_v<T, int>) {
        std::string_view sv = val.value();
        if (!sv.empty() && sv != "-") {
          dest = std::stoi(std::string(sv));
        }
      } else if constexpr (std::is_same_v<T, int64_t> ||
                           std::is_same_v<T, long long>) {
        std::string_view sv = val.value();
        if (!sv.empty() && sv != "-") {
          dest = std::stoll(std::string(sv));
        }
      } else if constexpr (std::is_same_v<T, double>) {
        std::string_view sv = val.value();
        if (!sv.empty() && sv != "-") {
          dest = std::stod(std::string(sv));
        }
      }
    } catch (...) {
      // Ignore malformed or unconvertible numbers
    }
  }
}

std::vector<OrderBook> NorenRestApi::get_order_book() {
  std::vector<OrderBook> order_book;

  std::string jData;
  jData.reserve(128);
  jData += "jData={\"uid\":\"";
  jData += credentials_.user_id;
  jData += "\"}";

  std::string response_text;
  bool ok = post_request("/OrderBook", jData, &response_text);

  if (!ok || response_text.empty()) {
    return order_book;
  }

  try {
    simdjson::ondemand::parser parser;
    simdjson::padded_string padded_json(response_text);
    simdjson::ondemand::document doc = parser.iterate(padded_json);

    auto array_result = doc.get_array();
    if (array_result.error()) {
      std::cout << "[INFO] No order in OrderBook today. \n";
      return order_book;
    }

    for (simdjson::ondemand::object item : array_result) {
      OrderBook entry{};

      get_field(item, "norenordno", entry.order_no);
      get_field(item, "tsym", entry.trading_symbol);
      get_field(item, "exch", entry.exchange);
      get_field(item, "status", entry.status);
      get_field(item, "trantype", entry.side);
      get_field(item, "prctyp", entry.price_type);
      get_field(item, "prd", entry.product);
      get_field(item, "qty", entry.quantity);
      get_field(item, "prc", entry.price);
      get_field(item, "fillshares", entry.filled_qty);
      get_field(item, "avgprc", entry.avg_price);
      get_field(item, "rejreason", entry.rejection_reason);
      get_field(item, "remarks", entry.remarks);
      get_field(item, "norentm", entry.order_time);

      order_book.push_back(entry);
    }
  } catch (const simdjson::simdjson_error &e) {
    std::cerr << "[SIMDJSON ERROR] " << e.what() << '\n';
  }

  return order_book;
}

std::vector<OptionChainItem>
NorenRestApi::get_option_chain(const std::string &exchange,
                               const std::string &symbol, double center_strike,
                               int count) {
  std::vector<OptionChainItem> chain;

  std::string jData;
  jData.reserve(256);
  jData += "jData={\"uid\":\"";
  jData += credentials_.user_id;
  jData += "\",\"exch\":\"";
  jData += exchange;
  jData += "\",\"tsym\":\"";
  jData += symbol;
  jData += "\",\"strprc\":\"";
  jData += std::to_string(center_strike);
  jData += "\",\"cnt\":\"";
  jData += std::to_string(count);
  jData += "\"}";

  std::string response_text;
  bool ok = post_request("/GetOptionChain", jData, &response_text);

  if (!ok || response_text.empty()) {
    return chain;
  }

  try {
    simdjson::ondemand::parser parser;
    simdjson::padded_string padded_json(response_text);
    simdjson::ondemand::document doc = parser.iterate(padded_json);

    auto values_array = doc["values"].get_array();
    if (values_array.error()) {
      std::cout << "[INFO] No option chain values found.\n";
      return chain;
    }

    for (simdjson::ondemand::object item : values_array) {
      OptionChainItem opt{};

      get_field(item, "tsym", opt.trading_symbol);
      get_field(item, "strprc", opt.strike_price);
      get_field(item, "optt", opt.option_type);
      get_field(item, "token", opt.token);
      get_field(item, "lp", opt.last_price);
      get_field(item, "oi", opt.open_interest);

      chain.push_back(opt);
    }
  } catch (const simdjson::simdjson_error &e) {
    std::cerr << "[SIMDJSON ERROR] " << e.what() << '\n';
  }

  return chain;
}

Quote NorenRestApi::get_quotes(const std::string &exchange,
                               const std::string &token) {
  Quote q{};

  std::string jData;
  jData.reserve(128);
  jData += "jData={\"uid\":\"";
  jData += credentials_.user_id;
  jData += "\",\"exch\":\"";
  jData += exchange;
  jData += "\",\"token\":\"";
  jData += token;
  jData += "\"}";

  std::string response_text;
  bool ok = post_request("/GetQuotes", jData, &response_text);

  if (!ok || response_text.empty()) {
    return q;
  }

  try {
    simdjson::ondemand::parser parser;
    simdjson::padded_string padded_json(response_text);
    simdjson::ondemand::document doc = parser.iterate(padded_json);
    simdjson::ondemand::object item = doc.get_object();

    q.exchange = exchange;
    q.token = token;

    get_field(item, "tsym", q.trading_symbol);
    get_field(item, "lp", q.lp);
    get_field(item, "o", q.open);
    get_field(item, "h", q.high);
    get_field(item, "l", q.low);
    get_field(item, "c", q.close);
    get_field(item, "cng", q.change);
    get_field(item, "nc", q.net_change_pct);
    get_field(item, "v", q.volume);
    get_field(item, "oi", q.open_interest);
    get_field(item, "exd", q.expiry_date);
    get_field(item, "bp1", q.best_bid_price);
    get_field(item, "bq1", q.best_bid_qty);
    get_field(item, "sp1", q.best_ask_price);
    get_field(item, "sq1", q.best_ask_qty);

  } catch (const simdjson::simdjson_error &e) {
    std::cerr << "[SIMDJSON ERROR in get_quotes] " << e.what() << '\n';
  }

  return q;
}

std::vector<TradeBook> NorenRestApi::get_trade_book() {
  std::vector<TradeBook> trade_book;

  std::string jData;
  jData += "jData={\"uid\":\"";
  jData += credentials_.user_id;
  jData += "\",\"actid\":\"";
  jData += credentials_.user_id;
  jData += "\"}";

  std::string response_text;

  bool ok = post_request("/TradeBook", jData, &response_text);

  if (!ok || response_text.empty()) {
    return trade_book;
  }

  try {
    simdjson::ondemand::parser parser;
    simdjson::padded_string padded_json(response_text);
    simdjson::ondemand::document doc = parser.iterate(padded_json);

    auto array_result = doc.get_array();
    if (array_result.error()) {
#ifdef NDEBUG
      std::cout << "[INFO] NO trades in TradeBook Today.\n";
#endif
      return trade_book;
    }

    for (simdjson::ondemand::object item : array_result) {
      TradeBook trade{};

      get_field(item, "norenordno", trade.order_no);
      get_field(item, "exch", trade.exchange);
      get_field(item, "tsym", trade.trading_symbol);
      get_field(item, "trantype", trade.side);
      get_field(item, "prd", trade.product);
      get_field(item, "prctyp", trade.price_type);
      get_field(item, "flid", trade.fill_id);
      get_field(item, "fltm", trade.fill_time);
      get_field(item, "flqty", trade.fill_qty);
      get_field(item, "flprc", trade.fill_price);
      get_field(item, "qty", trade.order_qty);
      get_field(item, "fillshares", trade.total_filled_qty);
      get_field(item, "avgprc", trade.avg_price);
      get_field(item, "exchordid", trade.exchange_order_id);
      get_field(item, "remarks", trade.remarks);

      trade_book.push_back(trade);
    }
  } catch (const simdjson::simdjson_error &e) {
    std::cerr << "[SIMDJSON ERROR in get_trade_book] " << e.what() << '\n';
  }
  return trade_book;
}
} // namespace shoonyacpp
