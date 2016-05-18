
#include "http_message.hpp"
#include "tcp.hpp"

namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    message<SendMsg,RecvMsg>::message(const std::shared_ptr<http::connection<SendMsg, RecvMsg>>& conn, std::uint32_t stream_id)
      : connection_(conn),
        stream_id_(stream_id)
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    message<SendMsg,RecvMsg>::message(message&& source)
      : connection_(source.connection_),
      stream_id_(source.stream_id_)
    {
      source.connection_ = nullptr;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    message<SendMsg,RecvMsg>::~message()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    std::uint32_t message<SendMsg,RecvMsg>::stream_id() const
    {
      return this->stream_id_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    ::manifold::http::version message<SendMsg,RecvMsg>::http_version() const
    {
      if (this->connection_)
        return this->connection_->version();
      else
        return http::version::unknown;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void message<SendMsg,RecvMsg>::cancel()
    {
      if (this->connection_)
        this->connection_->send_reset_stream(this->stream_id_, v2_errc::cancel);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void message<SendMsg,RecvMsg>::on_close(const std::function<void(const std::error_code&)>& cb)
    {
      if (this->connection_)
        this->connection_->on_close(this->stream_id_, cb);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template class message<response_head, request_head>;
    template class message<request_head, response_head>;
    //----------------------------------------------------------------//

//    //----------------------------------------------------------------//
//    std::string message::errorMessage() const
//    {
//      switch (this->error_code_)
//      {
//        case error_code::SocketError: return "Socket Error";
//        case error_code::HeadCorrupt: return "Headers Are Corrupt";
//        case error_code::HeadTooLarge: return "Headers Are Too Large";
//        default: return "";
//      }
//    }
//    //----------------------------------------------------------------//
  }
}