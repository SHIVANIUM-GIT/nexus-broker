#include "shoonyacpp/websocket.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <iostream>
#include <thread>
#include <string>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

namespace shoonyacpp {

struct NorenWebsocketImpl {
    net::io_context ioc;
    ssl::context ctx{ssl::context::tlsv12_client};
    std::unique_ptr<websocket::stream<beast::ssl_stream<tcp::socket>>> ws;
};

NorenWebsocket::NorenWebsocket(const std::string &ws_endpoint,
                               const UserCredentials &credentials)
    : ws_endpoint_(ws_endpoint), credentials_(credentials),
      ws_session_(new NorenWebsocketImpl()), is_running_(false) {
    auto impl = static_cast<NorenWebsocketImpl*>(ws_session_);
    impl->ctx.set_default_verify_paths();
    impl->ctx.set_verify_mode(ssl::verify_peer);
}

NorenWebsocket::~NorenWebsocket() {
    disconnect();
    delete static_cast<NorenWebsocketImpl*>(ws_session_);
}

void NorenWebsocket::connect() {
  if (is_running_) return;

  is_running_ = true;
  background_thread_ = std::thread([this] {
    auto impl = static_cast<NorenWebsocketImpl*>(ws_session_);
    try {
        std::string host = "api.shoonya.com";
        std::string port = "443";
        std::string path = "/NorenWSAPI/";

        tcp::resolver resolver(impl->ioc);
        impl->ws = std::make_unique<websocket::stream<beast::ssl_stream<tcp::socket>>>(impl->ioc, impl->ctx);

        auto const results = resolver.resolve(host, port);
        auto ep = net::connect(impl->ws->next_layer().next_layer(), results);

        if(!SSL_set_tlsext_host_name(impl->ws->next_layer().native_handle(), host.c_str())) {
            throw beast::system_error(
                beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()),
                "Failed to set SNI Hostname");
        }

        impl->ws->next_layer().handshake(ssl::stream_base::client);
        
        // Shoonya requires specific Origin header sometimes, setting standard headers
        impl->ws->set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req) {
                req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            }));

        impl->ws->handshake(host, path);
        std::cout << "[WS] Successfully connected to Shoonya (Boost.Beast TLS)\n";

        // Send Login frame
        std::string login_json = "{\"t\":\"c\",\"uid\":\"" + credentials_.user_id + 
                                 "\",\"actid\":\"" + credentials_.user_id + 
                                 "\",\"source\":\"API\",\"susertoken\":\"" + credentials_.access_token + "\"}";
        
        impl->ws->write(net::buffer(login_json));

        // Read Loop
        beast::flat_buffer buffer;
        while (is_running_) {
            impl->ws->read(buffer);
            std::string msg = beast::buffers_to_string(buffer.data());
            buffer.consume(buffer.size());
            
            if (on_tick_callback_) {
                on_tick_callback_(msg);
            }
        }

    } catch (std::exception const& e) {
        std::cerr << "[WS ERROR] " << e.what() << "\n";
        is_running_ = false;
    }
  });
}

void NorenWebsocket::disconnect() {
  if (!is_running_) return;

  is_running_ = false;
  auto impl = static_cast<NorenWebsocketImpl*>(ws_session_);
  
  if (impl->ws) {
      try {
          impl->ws->close(websocket::close_code::normal);
      } catch(...) {}
  }

  if (background_thread_.joinable()) {
    background_thread_.join();
  }
}

void NorenWebsocket::set_on_tick_callback(
    std::function<void(const std::string &tick_json)> callback) {
  on_tick_callback_ = std::move(callback);
}

void NorenWebsocket::subscribe(const std::string &instrument) {
  if (!is_running_) return;
  auto impl = static_cast<NorenWebsocketImpl*>(ws_session_);
  
  std::string sub_json = "{\"t\":\"t\",\"k\":\"" + instrument + "\"}";
  try {
      impl->ws->write(net::buffer(sub_json));
  } catch(std::exception const& e) {
      std::cerr << "[WS SUBSCRIBE ERROR] " << e.what() << "\n";
  }
}

} // namespace shoonyacpp