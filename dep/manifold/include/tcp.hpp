#pragma once

#ifndef MANIFOLD_TCP_HPP
#define MANIFOLD_TCP_HPP

#include "asio.hpp"
#include "socket.hpp"

//################################################################//
namespace manifold
{
  //================================================================//
  class tcp
  {
  private:
    static void recvline(asio::ip::tcp::socket& sock, char* buf, std::size_t bufSize, std::size_t putPosition,
                         char* bufEnd,
                         const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& handler,
                         const std::string& delim = "\r\n");
    static void recvline(asio::ssl::stream<asio::ip::tcp::socket>& ssl_stream, char* buf, std::size_t bufSize, std::size_t putPosition, char* bufEnd,
                         const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& handler,
                         const std::string& delim = "\r\n");
    tcp() = delete;
  public:
    //----------------------------------------------------------------//
//    static Socket connect(unsigned short port, const std::string& host, std::int64_t microseconds = 0);
//    template< class Rep, class Period>
//    static Socket connect(unsigned short port, const std::string& host, const std::chrono::duration<Rep,Period>& timeout = 0)
//    {
//      return TCP::connect(port, host, std::chrono::duration_cast<std::chrono::microseconds>(timeout).count());
//    }
//    static Socket listen(unsigned short port, const std::string& host = "", int backlog = SOMAXCONN);
//    ssize_t recvline(Socket& sock, char* buf, std::size_t bufSize, const std::string& delim = "\r\n");
    static void recvline(asio::ip::tcp::socket& sock, char* buf, std::size_t bufSize, const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& handler, const std::string& delim = "\r\n");
    static void recvline(asio::ssl::stream<asio::ip::tcp::socket>& ssl_stream, char* buf, std::size_t bufSize, const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& handler, const std::string& delim = "\r\n");


    // I may make this a Socket member in the future since socket errors are the only case where it would fail.
    //static bool sendAll(Socket& sock, const char* buf, std::size_t bufSize);
    //----------------------------------------------------------------//
  };
  //================================================================//
}
//################################################################//

#endif // MANIFOLD_TCP_HPP