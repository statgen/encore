#pragma once

#ifndef MANIFOLD_HTTP_CONNECTION_HPP
#define MANIFOLD_HTTP_CONNECTION_HPP

#include "http_request_head.hpp"
#include "http_response_head.hpp"
#include "http_error_category.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <iostream>

#define MANIFOLD_REMOVED_PRIORITY
#define MANIFOLD_REMOVED_TRAILERS

#ifdef MANIFOLD_DISABLE_HTTP2
#define MANIFOLD_HTTP_ALPN_SUPPORTED_PROTOCOLS "\x08http/1.1"
#else
#define MANIFOLD_HTTP_ALPN_SUPPORTED_PROTOCOLS "\x2h2\x08http/1.1"
#endif

namespace manifold
{
  namespace http
  {
    //================================================================//
    enum class version
    {
      unknown = 0,
      http1_1,
      http2
    };
    //================================================================//

    //================================================================//
    template <typename SendMsg, typename RecvMsg>
    class connection : public std::enable_shared_from_this<connection<SendMsg, RecvMsg>>
    {
    public:
      connection() {}
      virtual ~connection()
      {
        std::cout << "~connection()" << std::endl;
      }
      //----------------------------------------------------------------//
      //virtual void run() = 0;
      virtual ::manifold::http::version version() const = 0;
      virtual void close(const std::error_code& ec) = 0;
      virtual bool is_closed() const = 0;
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      virtual void on_new_stream(const std::function<void(std::uint32_t stream_id)>& fn) = 0;
      virtual void on_close(const std::function<void(const std::error_code& ec)>& fn) = 0;
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      virtual void on_headers(std::uint32_t stream_id, const std::function<void(RecvMsg&& headers)>& fn) = 0;
      virtual void on_informational_headers(std::uint32_t stream_id, const std::function<void(RecvMsg&& headers)>& fn) = 0;
#ifndef MANIFOLD_REMOVED_TRAILERS
      virtual void on_trailers(std::uint32_t stream_id, const std::function<void(header_block&& headers)>& fn) = 0;
#endif
      virtual void on_data(std::uint32_t stream_id, const std::function<void(const char* const buf, std::size_t buf_size)>& fn) = 0;
      virtual void on_close(std::uint32_t stream_id, const std::function<void(const std::error_code& ec)>& fn) = 0;
      virtual void on_push_promise(std::uint32_t stream_id, const std::function<void(SendMsg&& headers, std::uint32_t promised_stream_id)>& fn) = 0;


      virtual void on_end(std::uint32_t stream_id, const std::function<void()>& fn) = 0;
      virtual void on_drain(std::uint32_t stream_id, const std::function<void()>& fn) = 0;
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      virtual std::uint32_t create_stream(std::uint32_t dependency_stream_id, std::uint32_t stream_id) = 0;
      virtual bool send_data(std::uint32_t stream_id, const char *const data, std::uint32_t data_sz, bool end_stream) = 0;
      virtual bool send_headers(std::uint32_t stream_id, const SendMsg& head, bool end_headers, bool end_stream) = 0;
#ifndef MANIFOLD_REMOVED_TRAILERS
      virtual bool send_trailers(std::uint32_t stream_id, const header_block& head, bool end_headers, bool end_stream) = 0;
#endif
      //virtual bool send_headers(std::uint32_t stream_id, const v2_header_block& head, priority_options priority, bool end_headers, bool end_stream) = 0;
      //virtual bool send_priority(std::uint32_t stream_id, priority_options options) = 0;
      virtual void send_reset_stream(std::uint32_t stream_id, http::v2_errc error_code) = 0;
      virtual std::uint32_t send_push_promise(std::uint32_t stream_id, const RecvMsg& head) = 0;
      virtual void send_goaway(http::v2_errc error_code, const char *const data = nullptr, std::uint32_t data_sz = 0) = 0;
      //----------------------------------------------------------------//
    };
    //================================================================//
  }
}

#endif //MANIFOLD_HTTP_CONNECTION_HPP