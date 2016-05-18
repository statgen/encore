#pragma once

#ifndef MANIFOLD_HTTP_INCOMING_MESSAGE_HPP
#define MANIFOLD_HTTP_INCOMING_MESSAGE_HPP

#include "socket.hpp"
#include "http_message.hpp"

namespace manifold
{
  namespace http
  {
    //================================================================//
    template <typename SendMsg, typename RecvMsg>
    class incoming_message : public message<SendMsg, RecvMsg>
    {
    public:
      //----------------------------------------------------------------//
      incoming_message(const std::shared_ptr<http::connection<SendMsg, RecvMsg>>& conn, std::int32_t stream_id);
      incoming_message(incoming_message&& source);
      virtual ~incoming_message();

#ifndef MANIFOLD_REMOVED_TRAILERS
      const header_block& trailers() const { return this->trailers_; }
#endif
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      void on_data(const std::function<void(const char*const buff, std::size_t buff_size)>& fn);
      void on_end(const std::function<void()>& fn);
      //----------------------------------------------------------------//
    private:
      header_block trailers_;
    };
    //================================================================//
  }
}

#endif //MANIFOLD_HTTP_INCOMING_MESSAGE_HPP
