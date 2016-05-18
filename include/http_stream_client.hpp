#pragma once
#ifndef MANIFOLD_HTTP_STREAM_CLIENT_HPP
#define MANIFOLD_HTTP_STREAM_CLIENT_HPP

#include "http_client.hpp"
#include "uniform_resource_identifier.hpp"

namespace manifold
{
  namespace http
  {
    enum class response_status_errc : std::uint16_t
    {
      unknown_redirection_status = 1,
      unknown_client_error,
      unknown_server_error,

      // redirection
      multiple_choices                = 300,
      moved_permanently               = 301,
      found                           = 302,
      see_other                       = 303,
      not_modified                    = 304,
      use_proxy                       = 305,

      temporary_redirect              = 307,

      // client error
      bad_request                     = 400,
      unauthorized                    = 401,
      payment_required                = 402,
      forbidden                       = 403,
      not_found                       = 404,
      method_not_allowed              = 405,
      not_acceptable                  = 406,
      proxy_authentication_required   = 407,
      request_timeout                 = 408,
      conflict                        = 409,
      gone                            = 410,
      length_required                 = 411,
      precondition_failed             = 412,
      request_entity_too_large        = 413,
      request_uri_too_long            = 414,
      unsupported_media_type          = 415,
      requested_range_not_satisfiable = 416,
      expectation_failed              = 417,

      // server error
      internal_server_error           = 500,
      not_implemented                 = 501,
      bad_gateway                     = 502,
      service_unavailable             = 503,
      gateway_timeout                 = 504,
      http_version_not_supported      = 505
    };
  }
}

namespace std
{
  template<> struct is_error_code_enum<manifold::http::response_status_errc> : public true_type {};
}

namespace manifold
{
  namespace http
  {
    class response_status_error_category_impl : public std::error_category
    {
    public:
      response_status_error_category_impl() {}
      ~response_status_error_category_impl() {}
      const char* name() const noexcept;
      std::string message(int ev) const;
    };

    std::error_code make_error_code(manifold::http::response_status_errc e);

    class stream_client
    {
    public:
      typedef std::function<void(std::uint64_t, std::uint64_t)> progress_callback;
    private:
      class promise_impl
      {
      public:
        void update_send_progress(std::uint64_t, std::uint64_t);
        void update_recv_progress(std::uint64_t, std::uint64_t);
        void fulfill(const std::error_code&, const response_head&);
        void cancel();
        void on_send_progress(const progress_callback&);
        void on_recv_progress(const progress_callback&);
        void on_complete(const std::function<void(const std::error_code&, const response_head&)>& fn);
        void on_cancel(const std::function<void()>&);
      private:
        bool fulfilled_ = false;
        bool cancelled_ = false;
        std::function<void(const std::error_code& ec, const response_head& headers)> on_complete_;
        std::function<void()> on_cancel_;
        progress_callback on_send_progress_;
        progress_callback on_recv_progress_;
        std::error_code ec_;
        response_head headers_;
      };
    public:
      class promise
      {
      public:
        promise(const std::shared_ptr<promise_impl>& impl);
        void on_send_progress(const std::function<void(std::uint64_t bytes_transferred, std::uint64_t bytes_total)>& send_progress_cb);
        void on_recv_progress(const std::function<void(std::uint64_t bytes_transferred, std::uint64_t bytes_total)>& recv_progress_cb);
        void on_complete(const std::function<void(const std::error_code& ec, const response_head& res_head)>& fn);
        void cancel();
      private:
        std::shared_ptr<promise_impl> impl_;
      };

      stream_client(client& c);
      ~stream_client();

      promise send_request(const std::string& method, const uri& request_url, std::ostream& res_entity);
      promise send_request(const std::string& method, const uri& request_url, const std::list<std::pair<std::string,std::string>>& header_list, std::ostream& res_entity);
      promise send_request(const std::string& method, const uri& request_url, std::istream& req_entity, std::ostream& res_entity);
      promise send_request(const std::string& method, const uri& request_url, const std::list<std::pair<std::string,std::string>>& header_list, std::istream& req_entity, std::ostream& res_entity);

      void reset_max_redirects(std::uint8_t value = 5);
    private:
      client& client_;
      std::uint8_t max_redirects_;

      promise send_request(const std::string& method, const uri& request_url, const std::list<std::pair<std::string,std::string>>& header_list, std::ostream& res_entity, std::uint8_t max_redirects);
      promise send_request(const std::string& method, const uri& request_url, const std::list<std::pair<std::string,std::string>>& header_list, std::istream& req_entity, std::ostream& res_entity, std::uint8_t max_redirects);

      void handle_request(const std::error_code& ec, client::request&& req, const std::string& method, const uri& request_url, const std::list<std::pair<std::string,std::string>>& header_list, std::istream* req_entity, std::ostream* resp_entity, std::uint8_t max_redirects, const std::shared_ptr<promise_impl>& prom);
    };
  }
}

#endif //MANIFOLD_HTTP_STREAM_CLIENT_HPP
