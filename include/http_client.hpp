#pragma once

#ifndef MANIFOLD_HTTP_CLIENT_HPP
#define MANIFOLD_HTTP_CLIENT_HPP

#include <memory>
#include <set>
#include <functional>
//#include <initializer_list>
#include <forward_list>
#include <map>
#include <iostream>

#include "http_v1_connection.hpp"
#include "http_v2_connection.hpp"
#include "http_request_head.hpp"
#include "http_response_head.hpp"
#include "http_outgoing_message.hpp"
#include "http_incoming_message.hpp"

//class remote_match
//{
//public:
//  remote_match(const std::string& k, const std::string& v) {}
//  remote_match(const std::string& k, std::initializer_list<std::string> or_list) {}
//};
//
//class q
//{
//public:
//  q(std::string tbl) {}
//  q& match(std::initializer_list<remote_match> match_list)
//  {
//    return (*this);
//  }
//};
//
//void foo()
//{
//  //std::map<std::string,std::string> foo = {{"k","v"}};
//
//  //std::string str{std::string("foobar"),std::string("fomanchu")};
//
//  auto a = q("users")
//  .match(
//      {
//          {"last_name","Doe"},
//          {"first_name", {"John", "Jane"}},
//          {"age", {"30","31","32","33","34","35","36","37","38","39","40"}},
//          {"uuid","AF9E783C"}
//      });
//  std::cout << &a << std::endl;
//};

namespace manifold
{
  namespace http
  {
    class endpoint;

    class non_tls_session;

    class tls_session;
  }
}

namespace std
{
  template <>
  struct hash<manifold::http::endpoint>
  {
  public:
    size_t operator()(const manifold::http::endpoint& ep) const;
  };
}

namespace manifold
{
  namespace http
  {
    //================================================================//
    class client
    {
    public:
      //================================================================//
      class response : public incoming_message<request_head, response_head>
      {
      public:
        response(response_head&& head, const std::shared_ptr<http::connection<request_head, response_head>>& conn, std::uint32_t stream_id);
        response(response&& source);
        ~response();

        const response_head& head() const;
      private:
        response_head head_;
      protected:
        response_head& message_head() { return this->head_; }
      };
      //================================================================//

      //================================================================//
      class request : public outgoing_message<request_head, response_head>
      {
      public:
        request(request_head&& head, const std::shared_ptr<http::connection<request_head, response_head>>& conn, std::uint32_t stream_id, const std::string& server_authority);
        request(request&& source);
        ~request();

        request_head& head();

        bool send_headers(bool end_stream = false);

        void on_push_promise(const std::function<void(http::client::request && request)>& cb);
        void on_response(const std::function<void(http::client::response && resp)>& cb);
        void on_informational_headers(const std::function<void(response_head&& resp_head)>& cb);
      private:
        request_head head_;
        std::string server_authority_;
      protected:
        request_head& message_head() { return this->head_; }
      };
      //================================================================//

      client(asio::io_service& ioservice, asio::ssl::context& ctx);
      ~client();

      void reset_timeout(std::chrono::system_clock::duration value = std::chrono::system_clock::duration::max());

      void shutdown();
      void make_request(const std::string& host, std::uint16_t port, const std::function<void(const std::error_code& connect_error, request&& req)>& cb);
      void make_secure_request(const std::string& host, std::uint16_t port, const std::function<void(const std::error_code& connect_error, request&& req)>& cb);
    private:
      asio::io_service& io_service_;
      asio::ssl::context& ssl_ctx_;
      asio::ip::tcp::resolver tcp_resolver_;
      std::unordered_multimap<endpoint,std::shared_ptr<non_tls_session>> non_tls_sessions_;
      std::unordered_multimap<endpoint,std::shared_ptr<tls_session>> tls_sessions_;
      std::chrono::system_clock::duration timeout_;

      void handle_non_tls_session_close(const std::error_code& ec, const std::shared_ptr<non_tls_session>& sess);
      void handle_tls_session_close(const std::error_code& ec, const std::shared_ptr<tls_session>& sess);
    };
    //================================================================//

#if 0 // OLD CLIENT
    class client_impl;

    //================================================================//
    class client
    {
    public:
#ifndef MANIFOLD_DISABLE_HTTP2
      //================================================================//
      class v2_connection : public http::v2_connection<request_head, response_head>
      {
      public:
        v2_connection(non_tls_socket&& sock)
          : http::v2_connection<request_head, response_head>(std::move(sock))
        {
          this->local_settings_[setting_code::enable_push] = 1;
        }
        v2_connection(tls_socket&& sock)
          : http::v2_connection<request_head, response_head>(std::move(sock))
        {
          this->local_settings_[setting_code::enable_push] = 1;
        }
        ~v2_connection() {}

        //void on_informational_headers(std::uint32_t stream_id, const std::function<void(v2_response_head&& headers)>& fn);
        //void on_response(std::uint32_t stream_id, const std::function<void(http::client::response && resp)>& fn);
        //void on_trailers(std::uint32_t stream_id, const std::function<void(v2_header_block&& headers)>& fn);
        //void on_push_promise(std::uint32_t stream_id, const std::function<void(http::client::request && resp)>& fn);
      private:
//        class stream : public http::v2_connection<request_head, response_head>::stream
//        {
//        public:
//          stream(std::uint32_t stream_id, uint32_t initial_window_size, uint32_t initial_peer_window_size)
//            : http::v2_connection<request_head, response_head>::stream(stream_id, initial_window_size, initial_peer_window_size)
//          {
//            //http::v2_connection::stream::on_headers(std::bind(&stream::on_headers_handler, this, std::placeholders::_1));
//            //http::connection::stream::on_push_promise(std::bind(&stream::on_push_promise_handler, this, std::placeholders::_1, std::placeholders::_2));
//          }
//          ~stream() {}
//
//          //void on_informational_headers(const std::function<void(v2_response_head&& headers)>& fn) { this->on_informational_headers_ = fn; }
//          //void on_response_headers(const std::function<void(v2_response_head&& headers)>& fn) { this->on_response_headers_ = fn; }
//          //void on_trailers(const std::function<void(v2_header_block&& headers)>& fn) { this->on_trailers_ = fn; }
//          //void on_push_promise_headers(const std::function<void(v2_request_head&& headers, std::uint32_t)>& fn) { this->on_push_promise_repsonse_head_ = fn; }
//        private:
//          //std::function<void(v2_response_head&& headers)> on_informational_headers_;
//          //std::function<void(v2_response_head&& headers)> on_response_headers_;
//          //std::function<void(v2_header_block&& headers)> on_trailers_;
//          //std::function<void(v2_request_head&& req_head, std::uint32_t)> on_push_promise_repsonse_head_;
//
////          void on_headers_handler(v2_header_block&& headers)
////          {
////            // TODO: need to make sure response is only received once and that all of thes happen in the correct order.
////            int status_code = atoi(headers.header(":status").c_str());
////            if (status_code == 0)
////            {
////              this->on_trailers_ ? this->on_trailers_(std::move(headers)) : void();
////            }
////            else if (status_code < 200)
////            {
////              this->on_informational_headers_ ? this->on_informational_headers_(std::move(headers)) : void();
////            }
////            else
////            {
////              this->on_response_headers_ ? this->on_response_headers_(std::move(headers)) : void();
////            }
////          }
//        };
//
//
////        std::uint32_t next_stream_id_;
////
////        stream* create_stream_object(std::uint32_t stream_id = 0)
////        {
////          if (stream_id == 0 && this->next_stream_id_ <= v2_connection::max_stream_id)
////          {
////            stream_id = this->next_stream_id_;
////            this->next_stream_id_ += 2;
////          }
////
////          if (stream_id)
////            return new stream(stream_id, this->local_settings().at(setting_code::initial_window_size), this->peer_settings().at(setting_code::initial_window_size));
////          else
////            return nullptr;
////        }
//
      };
      //================================================================//
#endif //MANIFOLD_DISABLE_HTTP2
      //================================================================//
      class response : public incoming_message<request_head, response_head>
      {
      public:
        response(response_head&& head, const std::shared_ptr<http::connection<request_head, response_head>>& conn, std::uint32_t stream_id);
        response(response&& source);
        ~response();

        const response_head& head() const;
      private:
        response_head head_;
      protected:
        //----------------------------------------------------------------//
        response_head& message_head() { return this->head_; }
        //----------------------------------------------------------------//
      };
      //================================================================//

      //================================================================//
      class request : public outgoing_message<request_head, response_head>
      {
      public:
        request(request_head&& head, const std::shared_ptr<http::connection<request_head, response_head>>& conn, std::uint32_t stream_id, const std::string& server_authority);
        request(request&& source);
        ~request();

        request_head& head();

        bool send_headers(bool end_stream = false);

        void on_push_promise(const std::function<void(http::client::request && request)>& cb);
        void on_response(const std::function<void(http::client::response && resp)>& cb);
        void on_informational_headers(const std::function<void(response_head&& resp_head)>& cb);
      private:
        request_head head_;
        std::string server_authority_;
      protected:
        //----------------------------------------------------------------//
        request_head& message_head() { return this->head_; }
        //----------------------------------------------------------------//
      };
      //================================================================//

      //================================================================//
      struct ssl_options
      {
      public:
        asio::ssl::context::method method;
        ssl_options(asio::ssl::context::method meth = asio::ssl::context::method::tlsv12) : method(meth)
        {
        }
      };
      //================================================================//
    public:
      client(asio::io_service& ioservice, const std::string& host, unsigned short port = 80);
      client(asio::io_service& ioservice, const std::string& host, const ssl_options& options, unsigned short port = 443);
      client(client&& source);
      ~client();

      void on_connect(const std::function<void()>& fn);
      void on_close(const std::function<void(errc ec)>& fn);
      client::request make_request();
      void close(errc ec = errc::no_error);
      void set_default_user_agent(const std::string user_agent);
    private:
      client(const client&); //= delete;
      client& operator=(const client&); //= delete;
      client& operator=(client&&); //= delete;
      std::shared_ptr<client_impl> impl_;
    };
    //================================================================//
#endif
  }
}

#endif // MANIFOLD_HTTP_CLIENT_HPP