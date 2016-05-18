#pragma once

#ifndef MANIFOLD_HTTP_SERVER_HPP
#define MANIFOLD_HTTP_SERVER_HPP

#include <regex>
#include <functional>
#include <set>
#include <memory>
#include <ctime>

#include "http_v2_request_head.hpp"
#include "http_v2_response_head.hpp"
#include "http_outgoing_message.hpp"
#include "http_incoming_message.hpp"
#include "http_connection.hpp"

namespace manifold
{
  namespace http
  {
    //================================================================//
    class server
    {
    public:
      //================================================================//
      class request : public incoming_message<response_head, request_head>
      {
      public:
        //----------------------------------------------------------------//
        request(request_head&& head, const std::shared_ptr<http::connection<response_head, request_head>>& conn, std::int32_t stream_id);
        request(request&& source);
        ~request();
        //----------------------------------------------------------------//

        //----------------------------------------------------------------//
        const request_head& head() const;
        //----------------------------------------------------------------//
      private:
        //----------------------------------------------------------------//
        request_head head_;
        //----------------------------------------------------------------//
      protected:
        //----------------------------------------------------------------//
        //header_block& message_head() { return this->head_; }
        //----------------------------------------------------------------//
      };
      //================================================================//

      class push_promise;
      //================================================================//
      class response : public outgoing_message<response_head, request_head>
      {
      public:
        //----------------------------------------------------------------//
        response(response_head&& head, const std::shared_ptr<http::connection<response_head, request_head>>& conn, std::int32_t stream_id, const std::string& request_method, const std::string& request_authority);
        response(response&& source);
        ~response();
        //----------------------------------------------------------------//

        //----------------------------------------------------------------//
        response_head& head();
        bool send_headers(bool end_stream = false);
        push_promise send_push_promise(request_head&& push_promise_headers);
        push_promise send_push_promise(const request_head& push_promise_headers);
        //----------------------------------------------------------------//
      private:
        //----------------------------------------------------------------//
        response_head head_;
        std::string request_method_;
        std::string request_authority_;
        //----------------------------------------------------------------//
      protected:
        //----------------------------------------------------------------//
        response_head& message_head() { return this->head_; }
        //----------------------------------------------------------------//
      };
      //================================================================//

      //================================================================//
      class push_promise
      {
      public:
        push_promise();
        push_promise(request&& req, response&& res);
        void fulfill(const std::function<void(request&& req, response&& res)>& handler);
      private:
        std::unique_ptr<request> req_;
        std::unique_ptr<response> res_;
        bool fulfilled_;
      };
      //================================================================//

      //================================================================//
      struct ssl_options
      {
      public:
        asio::ssl::context::method method;
        std::vector<char> pfx;
        std::vector<char> key;
        std::vector<char> chain;
        std::vector<char> passphrase;
        std::vector<char> cert;
        std::vector<char> ca;
        std::vector<char> dhparam;
        ssl_options(asio::ssl::context::method meth) : method(meth)
        {
        }
      };
      //================================================================//
    public:
      //----------------------------------------------------------------//
      server(asio::io_service& ioService, unsigned short port = 80, const std::string& host = "0.0.0.0");
      server(asio::io_service& ioService, asio::ssl::context& ctx, unsigned short port = 443, const std::string& host = "0.0.0.0");
      ~server();
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      void reset_timeout(std::chrono::system_clock::duration value = std::chrono::system_clock::duration::max());
      void listen(const std::function<void(server::request&& req, server::response&& res)>& handler);
      void listen(const std::function<void(server::request&& req, server::response&& res)>& handler, std::error_code& ec);
      void close();
      //void register_handler(const std::regex& expression, const std::function<void(server::request&& req, server::response&& res)>& handler);
      void set_default_server_header(const std::string& value);
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      static std::string date_string()
      {
        const int RFC1123_TIME_LEN = 29;
        time_t t;
        struct tm* tm;
        char buf[RFC1123_TIME_LEN+1] = {'\0'};

        time(&t);
        tm = std::gmtime(&t);

        strftime(buf, RFC1123_TIME_LEN+1, "%a, %d %b %Y %H:%M:%S GMT", tm);
        return std::string(&buf[0]);
      }
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      static std::string date_string(time_t& t)
      {
        const int RFC1123_TIME_LEN = 29;
        struct tm* tm;
        char buf[RFC1123_TIME_LEN+1] = {'\0'};

        tm = std::gmtime(&t);

        strftime(buf, RFC1123_TIME_LEN+1, "%a, %d %b %Y %H:%M:%S GMT", tm);
        return std::string(&buf[0]);
      }
      //----------------------------------------------------------------//
    private:
      std::shared_ptr<class server_impl> impl_;
    };
    //================================================================//
  }
}

#endif // MANIFOLD_HTTP_SERVER_HPP