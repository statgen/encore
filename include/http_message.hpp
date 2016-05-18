#pragma once

#ifndef MANIFOLD_HTTP_MESSAGE_HPP
#define MANIFOLD_HTTP_MESSAGE_HPP

#include "http_v2_connection.hpp"
#include "http_v2_message_head.hpp"
#include "hpack.hpp"

namespace manifold
{
  namespace http
  {
    //================================================================//
    template <typename SendMsg, typename RecvMsg>
    class message
    {
    public:
      //----------------------------------------------------------------//
      message(const std::shared_ptr<http::connection<SendMsg, RecvMsg>>& conn, std::uint32_t stream_id);
      message(message&& source);
      virtual ~message();
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      std::uint32_t stream_id() const;
      ::manifold::http::version http_version() const;
      void cancel();
      void on_close(const std::function<void(const std::error_code& ec)>& cb);
      //----------------------------------------------------------------//
    private:
      message(const message&); //= delete;
      message& operator=(const message&); //= delete;
      message& operator=(message&&); //= delete;
    protected:
      //----------------------------------------------------------------//
      std::shared_ptr<http::connection<SendMsg, RecvMsg>> connection_;
      std::uint32_t stream_id_;
      //----------------------------------------------------------------//
    };
    //================================================================//
  }
}

#endif //MANIFOLD_HTTP_MESSAGE_HPP
