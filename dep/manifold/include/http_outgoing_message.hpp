#pragma once

#ifndef MANIFOLD_HTTP_OUTGOING_MESSAGE_HPP
#define MANIFOLD_HTTP_OUTGOING_MESSAGE_HPP

#include "http_message.hpp"

namespace manifold
{
  namespace http
  {
    //================================================================//
    template <typename SendMsg, typename RecvMsg>
    class outgoing_message : public message<SendMsg, RecvMsg>
    {
    public:
      //----------------------------------------------------------------//
      outgoing_message(const std::shared_ptr<http::connection<SendMsg,RecvMsg>>& conn, std::int32_t stream_id);
      outgoing_message(outgoing_message&& source);
      virtual ~outgoing_message();
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      virtual bool send_headers(bool end_stream = false); // Must be virtual since client::request and server::response override while outgoing_message::end/send call this method.
      bool send(const char*const data, std::size_t data_sz);
      bool send(const char* cstr) { return this->send(std::string(cstr)); }
      template <typename BufferT>
      bool send(const BufferT& dataBuffer)
      {
        return this->send(dataBuffer.data(), dataBuffer.size());
      }
      void on_drain(const std::function<void()>& fn);

#ifndef MANIFOLD_REMOVED_TRAILERS
      bool end(const char*const data, std::size_t data_sz, const header_block& trailers = {});
      bool end(const char* cstr, const header_block& trailers = {})
      {
        return this->end(std::string(cstr), trailers);
      }
      template <typename BufferT>
      bool end(const BufferT& dataBuffer, const header_block& trailers = {})
      {
        return this->end(dataBuffer.data(), dataBuffer.size(), trailers);
      }
      bool end(const header_block& trailers = {});
#else
      bool end(const char*const data, std::size_t data_sz);
      bool end(const char* cstr)
      {
        return this->end(std::string(cstr));
      }
      template <typename BufferT>
      bool end(const BufferT& dataBuffer)
      {
        return this->end(dataBuffer.data(), dataBuffer.size());
      }
      bool end();
#endif
      //----------------------------------------------------------------//
    private:
      //----------------------------------------------------------------//
      bool headers_sent_;
      bool ended_;
      //----------------------------------------------------------------//
    protected:
      //----------------------------------------------------------------//
      virtual SendMsg& message_head() = 0;
      //----------------------------------------------------------------//
    };
    //================================================================//
  }
}

#endif //MANIFOLD_HTTP_OUTGOING_MESSAGE_HPP
