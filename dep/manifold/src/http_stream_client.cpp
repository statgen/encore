
#include "http_stream_client.hpp"

namespace manifold
{
  namespace http
  {
    //================================================================//
    response_status_errc status_code_to_errc(std::uint16_t status_code)
    {
      if (status_code >= 300 && status_code < 400)
      {
        switch (status_code)
        {
          case static_cast<std::uint16_t>(response_status_errc::multiple_choices  ): return response_status_errc::multiple_choices   ;
          case static_cast<std::uint16_t>(response_status_errc::moved_permanently ): return response_status_errc::moved_permanently  ;
          case static_cast<std::uint16_t>(response_status_errc::found             ): return response_status_errc::found              ;
          case static_cast<std::uint16_t>(response_status_errc::see_other         ): return response_status_errc::see_other          ;
          case static_cast<std::uint16_t>(response_status_errc::not_modified      ): return response_status_errc::not_modified       ;
          case static_cast<std::uint16_t>(response_status_errc::use_proxy         ): return response_status_errc::use_proxy          ;
          case static_cast<std::uint16_t>(response_status_errc::temporary_redirect): return response_status_errc::temporary_redirect ;
          default: return response_status_errc::unknown_redirection_status;
        }
      }
      else if (status_code >= 400 && status_code < 500)
      {
        switch (status_code)
        {
          case static_cast<std::uint16_t>(response_status_errc::bad_request                    ): return response_status_errc::bad_request                     ;
          case static_cast<std::uint16_t>(response_status_errc::unauthorized                   ): return response_status_errc::unauthorized                    ;
          case static_cast<std::uint16_t>(response_status_errc::payment_required               ): return response_status_errc::payment_required                ;
          case static_cast<std::uint16_t>(response_status_errc::forbidden                      ): return response_status_errc::forbidden                       ;
          case static_cast<std::uint16_t>(response_status_errc::not_found                      ): return response_status_errc::not_found                       ;
          case static_cast<std::uint16_t>(response_status_errc::method_not_allowed             ): return response_status_errc::method_not_allowed              ;
          case static_cast<std::uint16_t>(response_status_errc::not_acceptable                 ): return response_status_errc::not_acceptable                  ;
          case static_cast<std::uint16_t>(response_status_errc::proxy_authentication_required  ): return response_status_errc::proxy_authentication_required   ;
          case static_cast<std::uint16_t>(response_status_errc::request_timeout                ): return response_status_errc::request_timeout                 ;
          case static_cast<std::uint16_t>(response_status_errc::conflict                       ): return response_status_errc::conflict                        ;
          case static_cast<std::uint16_t>(response_status_errc::gone                           ): return response_status_errc::gone                            ;
          case static_cast<std::uint16_t>(response_status_errc::length_required                ): return response_status_errc::length_required                 ;
          case static_cast<std::uint16_t>(response_status_errc::precondition_failed            ): return response_status_errc::precondition_failed             ;
          case static_cast<std::uint16_t>(response_status_errc::request_entity_too_large       ): return response_status_errc::request_entity_too_large        ;
          case static_cast<std::uint16_t>(response_status_errc::request_uri_too_long           ): return response_status_errc::request_uri_too_long            ;
          case static_cast<std::uint16_t>(response_status_errc::unsupported_media_type         ): return response_status_errc::unsupported_media_type          ;
          case static_cast<std::uint16_t>(response_status_errc::requested_range_not_satisfiable): return response_status_errc::requested_range_not_satisfiable ;
          case static_cast<std::uint16_t>(response_status_errc::expectation_failed             ): return response_status_errc::expectation_failed              ;
          default: return response_status_errc::unknown_client_error;
        }
      }
      else if (status_code >= 500 && status_code < 600)
      {
        switch (status_code)
        {
          case static_cast<std::uint16_t>(response_status_errc::internal_server_error     ): return response_status_errc::internal_server_error      ;
          case static_cast<std::uint16_t>(response_status_errc::not_implemented           ): return response_status_errc::not_implemented            ;
          case static_cast<std::uint16_t>(response_status_errc::bad_gateway               ): return response_status_errc::bad_gateway                ;
          case static_cast<std::uint16_t>(response_status_errc::service_unavailable       ): return response_status_errc::service_unavailable        ;
          case static_cast<std::uint16_t>(response_status_errc::gateway_timeout           ): return response_status_errc::gateway_timeout            ;
          case static_cast<std::uint16_t>(response_status_errc::http_version_not_supported): return response_status_errc::http_version_not_supported ;
          default: return response_status_errc::unknown_server_error;
        }
      }
      return response_status_errc::unknown_server_error;
    }

    const char* response_status_error_category_impl::name() const noexcept
    {
      return "Manifold Unsuccessful HTTP Response Status";
    }

    std::string response_status_error_category_impl::message(int ev) const
    {
      switch (ev)
      {
        case static_cast<int>(response_status_errc::unknown_redirection_status      ): return "unknown redirection status";
        case static_cast<int>(response_status_errc::unknown_client_error            ): return "unknown client error";
        case static_cast<int>(response_status_errc::unknown_server_error            ): return "unknown server error";
        case static_cast<int>(response_status_errc::multiple_choices                ): return "multiple choices";
        case static_cast<int>(response_status_errc::moved_permanently               ): return "moved permanently";
        case static_cast<int>(response_status_errc::found                           ): return "found";
        case static_cast<int>(response_status_errc::see_other                       ): return "see other";
        case static_cast<int>(response_status_errc::not_modified                    ): return "not modified";
        case static_cast<int>(response_status_errc::use_proxy                       ): return "use proxy";
        case static_cast<int>(response_status_errc::temporary_redirect              ): return "temporary redirect";
        case static_cast<int>(response_status_errc::bad_request                     ): return "bad request";
        case static_cast<int>(response_status_errc::unauthorized                    ): return "unauthorized";
        case static_cast<int>(response_status_errc::payment_required                ): return "payment required";
        case static_cast<int>(response_status_errc::forbidden                       ): return "forbidden";
        case static_cast<int>(response_status_errc::not_found                       ): return "not found";
        case static_cast<int>(response_status_errc::method_not_allowed              ): return "method not allowed";
        case static_cast<int>(response_status_errc::not_acceptable                  ): return "not acceptable";
        case static_cast<int>(response_status_errc::proxy_authentication_required   ): return "proxy authentication required";
        case static_cast<int>(response_status_errc::request_timeout                 ): return "request timeout";
        case static_cast<int>(response_status_errc::conflict                        ): return "conflict";
        case static_cast<int>(response_status_errc::gone                            ): return "gone";
        case static_cast<int>(response_status_errc::length_required                 ): return "length required";
        case static_cast<int>(response_status_errc::precondition_failed             ): return "precondition failed";
        case static_cast<int>(response_status_errc::request_entity_too_large        ): return "request entity too large";
        case static_cast<int>(response_status_errc::request_uri_too_long            ): return "request uri too long";
        case static_cast<int>(response_status_errc::unsupported_media_type          ): return "unsupported media type";
        case static_cast<int>(response_status_errc::requested_range_not_satisfiable ): return "requested range not satisfiable";
        case static_cast<int>(response_status_errc::expectation_failed              ): return "expectation failed";
        case static_cast<int>(response_status_errc::internal_server_error           ): return "internal server error";
        case static_cast<int>(response_status_errc::not_implemented                 ): return "not implemented";
        case static_cast<int>(response_status_errc::bad_gateway                     ): return "bad gateway";
        case static_cast<int>(response_status_errc::service_unavailable             ): return "service unavailable";
        case static_cast<int>(response_status_errc::gateway_timeout                 ): return "gateway timeout";
        case static_cast<int>(response_status_errc::http_version_not_supported      ): return "http version not supported";
      };
      return "Unknown Error";
    }

    const manifold::http::response_status_error_category_impl stream_client_error_category_object;
    std::error_code make_error_code (manifold::http::response_status_errc e)
    {
      return std::error_code(static_cast<int>(e), stream_client_error_category_object);
    }
    //================================================================//

    //================================================================//
    void stream_client::promise_impl::update_send_progress(std::uint64_t bytes_transferred, std::uint64_t bytes_total)
    {
      this->on_send_progress_ ? this->on_send_progress_(bytes_transferred, bytes_total) : void();
    }

    void stream_client::promise_impl::update_recv_progress(std::uint64_t bytes_transferred, std::uint64_t bytes_total)
    {
      this->on_recv_progress_ ? this->on_recv_progress_(bytes_transferred, bytes_total) : void();
    }

    void stream_client::promise_impl::fulfill(const std::error_code& ec, const response_head& headers)
    {
      if (!fulfilled_)
      {
        fulfilled_ = true;

        ec_ = ec;
        headers_ = headers;

        on_complete_ ? on_complete_(ec_, headers_) : void();
        on_complete_ = nullptr;
        on_cancel_ = nullptr;
        on_send_progress_ = nullptr;
        on_recv_progress_ = nullptr;
      }
    }

    void stream_client::promise_impl::cancel()
    {
      if (!cancelled_)
      {
        cancelled_ = true;

        on_cancel_ ? on_cancel_() : void();
      }
    }

    void stream_client::promise_impl::on_send_progress(const progress_callback& prog_fn)
    {
      if (!cancelled_ && !fulfilled_)
        this->on_send_progress_ = prog_fn;
    }

    void stream_client::promise_impl::on_recv_progress(const progress_callback& prog_fn)
    {
      if (!cancelled_ && !fulfilled_)
        this->on_recv_progress_ = prog_fn;
    }


    void stream_client::promise_impl::on_complete(const std::function<void(const std::error_code& ec, const response_head& headers)>& fn)
    {
      if (fulfilled_)
        fn ? fn(ec_, headers_) : void();
      else
        on_complete_ = fn;
    }

    void stream_client::promise_impl::on_cancel(const std::function<void()>& fn)
    {
      if (cancelled_)
        fn ? fn() : void();
      else if (!fulfilled_)
        on_cancel_ = fn;
    }
    //================================================================//

    //================================================================//
    stream_client::promise::promise(const std::shared_ptr<promise_impl>& impl)
      : impl_(impl)
    {
    }

    void stream_client::promise::on_send_progress(const progress_callback& prog_fn)
    {
      impl_->on_send_progress(prog_fn);
    }

    void stream_client::promise::on_recv_progress(const progress_callback& prog_fn)
    {
      impl_->on_recv_progress(prog_fn);
    }

    void stream_client::promise::on_complete(const std::function<void(const std::error_code& ec, const response_head& res_head)>& fn)
    {
      impl_->on_complete(fn);
    }

    void stream_client::promise::cancel()
    {
      impl_->cancel();
    }
    //================================================================//

    //================================================================//
    stream_client::stream_client(client& c)
      : client_(c)
    {
      this->reset_max_redirects();
    }

    stream_client::~stream_client()
    {

    }

    void stream_client::reset_max_redirects(std::uint8_t value)
    {
      this->max_redirects_ = value;
    }

    stream_client::promise stream_client::send_request(const std::string& method, const uri& request_url, std::ostream& res_entity)
    {
      return send_request(method, request_url, {}, res_entity, max_redirects_);
    }

    stream_client::promise stream_client::send_request(const std::string& method, const uri& request_url, const std::list<std::pair<std::string,std::string>>& header_list, std::ostream& res_entity)
    {
      return send_request(method, request_url, header_list, res_entity, max_redirects_);
    }

    stream_client::promise stream_client::send_request(const std::string& method, const uri& request_url, std::istream& req_entity, std::ostream& res_entity)
    {
      return send_request(method, request_url, {}, req_entity, res_entity, max_redirects_);
    }

    stream_client::promise stream_client::send_request(const std::string& method, const uri& request_url, const std::list<std::pair<std::string,std::string>>& header_list, std::istream& req_entity, std::ostream& res_entity)
    {
      return send_request(method, request_url, header_list, req_entity, res_entity, max_redirects_);
    }

    void stream_client::handle_request(const std::error_code& ec, client::request&& tmp_req, const std::string& method, const uri& request_url, const std::list<std::pair<std::string,std::string>>& header_list, std::istream* req_entity, std::ostream* resp_entity, std::uint8_t max_redirects, const std::shared_ptr<promise_impl>& prom)
    {
      if (ec)
      {
        prom->fulfill(ec, response_head());
      }
      else
      {
        auto req = std::make_shared<client::request>(std::move(tmp_req));

        req->on_close([prom](const std::error_code& ec)
        {
          prom->fulfill(ec, response_head());
        });

        prom->on_cancel([req]()
        {
          req->cancel();
        });

        req->on_response([this, prom, req_entity, resp_entity, max_redirects, method, request_url, header_list, req](client::response&& resp)
        {
          if (resp.head().has_successful_status())
          {
            if (resp_entity)
            {
              std::uint64_t content_length = 0;
              std::stringstream ss_content_length(resp.head().header("content-length"));
              ss_content_length >> content_length;
              auto total_bytes_received = std::make_shared<std::uint64_t>(0);
              resp.on_data([resp_entity, total_bytes_received, content_length, prom](const char*const data, std::size_t sz)
              {
                resp_entity->write(data, sz);
                (*total_bytes_received) += sz;
                prom->update_recv_progress(*total_bytes_received, content_length);
              });
            }

            auto headers = std::make_shared<response_head>(resp.head());
            resp.on_end([prom, headers]()
            {
              prom->fulfill(std::error_code(), *headers);
            });
          }
          else if (resp.head().has_redirection_status())
          {
            uri redirect_url = resp.head().header("location");
            if (!redirect_url.is_valid() || max_redirects == 0)
              prom->fulfill(status_code_to_errc(resp.head().status_code()), resp.head());
            else
            {
              if (redirect_url.is_relative())
              {
                redirect_url.host(request_url.host());
                redirect_url.port(request_url.port());
              }

              if (redirect_url.scheme_name().empty())
                redirect_url.scheme_name(request_url.scheme_name());

              prom->on_cancel(nullptr);
              req->on_close(nullptr);

              std::list<std::pair<std::string,std::string>> redirect_header_list;
              for (auto it = header_list.begin(); it != header_list.end(); ++it)
              {
                if (it->first != "authentication" // TODO: Add option to trust location header
                    && it->first != "cookie") // TODO: Add cookie manager.
                {
                  redirect_header_list.push_back(*it);
                }
              }

              if (redirect_url.scheme_name() == "https")
                this->client_.make_secure_request(redirect_url.host(), redirect_url.port(), std::bind(&stream_client::handle_request, this, std::placeholders::_1, std::placeholders::_2, method, redirect_url, redirect_header_list, req_entity, resp_entity, max_redirects - 1, prom));
              else
                this->client_.make_request(redirect_url.host(), redirect_url.port(), std::bind(&stream_client::handle_request, this, std::placeholders::_1, std::placeholders::_2, method, redirect_url, redirect_header_list, req_entity, resp_entity, max_redirects - 1, prom));
            }
          }
          else
          {
            prom->fulfill(status_code_to_errc(resp.head().status_code()), resp.head());
          }
        });

        req->head().method(method);
        req->head().path(request_url.path_with_query());
        for (auto it = header_list.begin(); it != header_list.end(); ++it)
          req->head().header(it->first, it->second);

        if (!req_entity)
        {
          req->end();
        }
        else
        {
          req_entity->clear();
          req_entity->seekg(0, std::ios::beg);

          std::uint64_t content_length = 0;
          std::stringstream ss_content_length(req->head().header("content-length"));
          ss_content_length >> content_length;
          auto total_bytes_sent = std::make_shared<std::uint64_t>(0);

          std::array<char, 8192> buf;
          long bytes_in_buf = req_entity->read(buf.data(), buf.size()).gcount();
          if (!req_entity->good())
          {
            if (bytes_in_buf > 0)
              req->end(buf.data(), (std::size_t)bytes_in_buf);
            else
              req->end();
          }
          else
          {
            req->on_drain([req_entity, req, total_bytes_sent, content_length, prom]()
            {
              std::array<char, 8192> buf;
              long bytes_in_buf = req_entity->read(buf.data(), buf.size()).gcount();
              if (bytes_in_buf > 0)
              {
                req->send(buf.data(), (std::size_t)bytes_in_buf);
                (*total_bytes_sent) += (std::size_t)bytes_in_buf;
                prom->update_send_progress(*total_bytes_sent, content_length);
              }

              if (!req_entity->good())
                req->end();
            });
            req->send(buf.data(), (std::size_t)bytes_in_buf);
            (*total_bytes_sent) += (std::size_t)bytes_in_buf;
            prom->update_send_progress(*total_bytes_sent, content_length);
          }
        }
      }
    }

    stream_client::promise stream_client::send_request(const std::string& method, const uri& request_url, const std::list<std::pair<std::string,std::string>>& header_list, std::ostream& resp_entity, std::uint8_t max_redirects)
    {
      auto prom = std::make_shared<promise_impl>();
      promise ret(prom);

      if (request_url.scheme_name() == "https")
        this->client_.make_secure_request(request_url.host(), request_url.port(), std::bind(&stream_client::handle_request, this, std::placeholders::_1, std::placeholders::_2, method, request_url, header_list, nullptr, &resp_entity, max_redirects, prom));
      else
        this->client_.make_request(request_url.host(), request_url.port(), std::bind(&stream_client::handle_request, this, std::placeholders::_1, std::placeholders::_2, method, request_url, header_list, nullptr, &resp_entity, max_redirects, prom));

      return ret;
    }

    stream_client::promise stream_client::send_request(const std::string& method, const uri& request_url, const std::list<std::pair<std::string,std::string>>& header_list, std::istream& req_entity, std::ostream& resp_entity, std::uint8_t max_redirects)
    {
      auto prom = std::make_shared<promise_impl>();
      promise ret(prom);

      if (request_url.scheme_name() == "https")
        this->client_.make_secure_request(request_url.host(), request_url.port(), std::bind(&stream_client::handle_request, this, std::placeholders::_1, std::placeholders::_2, method, request_url, header_list, &req_entity, &resp_entity, max_redirects, prom));
      else
        this->client_.make_request(request_url.host(), request_url.port(), std::bind(&stream_client::handle_request, this, std::placeholders::_1, std::placeholders::_2, method, request_url, header_list, &req_entity, &resp_entity, max_redirects, prom));

      return ret;
    }
    //================================================================//
  }
}