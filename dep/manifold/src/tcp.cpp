#include <errno.h>
#include <cstring>
#include <algorithm>
#include <iostream>

#include "tcp.hpp"

//################################################################//
namespace manifold
{
//  //----------------------------------------------------------------//
//  Socket TCP::connect(unsigned short port, const std::string& host, std::int64_t microseconds)
//  {
//    Socket ret;
//    struct addrinfo hints, *res;
//
//    memset(&hints, 0, sizeof hints);
//    hints.ai_family = AF_UNSPEC; //(int)Socket::Family::Unspecified;
//    hints.ai_socktype = SOCK_STREAM; //(int)Socket::Type::Stream;
//
//    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) == 0)
//    {
//      ret = Socket((Socket::Family)res->ai_family, (Socket::Type)res->ai_socktype, res->ai_protocol);
//      if (ret.fd() > -1)
//      {
//        if (!ret.connect(res->ai_addr, res->ai_addrlen, microseconds))
//        {
//          ret.close();
//        }
//      }
//
//      freeaddrinfo(res);
//    }
//
//    return ret;
//  }
//  //----------------------------------------------------------------//
//
//  //----------------------------------------------------------------//
//  Socket TCP::listen(unsigned short port, const std::string& host, int backlog)
//  {
//    Socket ret;
//
//    struct addrinfo hints, *res;
//    int getAddressRes;
//
//    memset(&hints, 0, sizeof hints);
//    hints.ai_family = AF_UNSPEC;
//    hints.ai_socktype = SOCK_STREAM;
//    if (host.empty() || host == "0.0.0.0")
//    {
//      hints.ai_flags = AI_PASSIVE;
//      getAddressRes = ::getaddrinfo(NULL, std::to_string(port).c_str(), &hints, &res);
//    }
//    else
//    {
//      getAddressRes = ::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res);
//    }
//
//    if (getAddressRes == 0)
//    {
//      ret = Socket((Socket::Family)res->ai_family, (Socket::Type)res->ai_socktype, res->ai_protocol);
//      if (ret.fd() > -1)
//      {
//        if (!ret.bind(res->ai_addr, res->ai_addrlen) || !ret.listen(backlog))
//          ret.close();
//      }
//
//      ::freeaddrinfo(res);
//    }
//
//
//    return ret;
//  }
//  //----------------------------------------------------------------//

//  //----------------------------------------------------------------//
//  ssize_t TCP::recvline(Socket& sock, char* buf, std::size_t bufSize, const std::string& delim)
//  {
//    ssize_t ret = -1;
//
//    const size_t discardBufferSize = 1024;
//
//    if (delim.size() <= bufSize)
//    {
//      bool delimFound = false;
//      char* bufEnd = buf;
//      std::size_t putPosition = 0;
//      while (bufSize && !delimFound)
//      {
//        std::size_t bytesToRead = (bufSize > discardBufferSize ? discardBufferSize : bufSize);
//        ssize_t bytesActuallyRead = sock.recv(&buf[putPosition], bytesToRead, MSG_PEEK);
//        if (bytesActuallyRead == -1)
//          break;
//        bufEnd += bytesActuallyRead;
//        char* searchResult = std::search(buf, bufEnd, delim.begin(), delim.end());
//        if (searchResult != bufEnd)
//        {
//          bufEnd = searchResult + delim.size();
//          delimFound = true;
//        }
//
//        char tmp[discardBufferSize];
//        size_t bytesToDiscard = bufEnd - &buf[putPosition];
//        sock.recv(tmp, bytesToDiscard); //TODO: Handle Error Case
//        bufSize -= bytesToDiscard;
//        putPosition += bytesToDiscard; //= bufEnd - buf;
//      }
//
//      if (delimFound)
//        ret = putPosition;
//    }
//    return ret;
//  }
//  //----------------------------------------------------------------//

  //----------------------------------------------------------------//
  void tcp::recvline(asio::ip::tcp::socket& sock, char* buf, std::size_t bufSize, std::size_t putPosition, char* bufEnd,
                     const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& cb,
                     const std::string& delim)
  {
    sock.async_receive(asio::null_buffers(), 0, [&sock, buf, bufSize, putPosition, bufEnd, delim, cb](const std::error_code& ec, std::size_t bytes_transferred) mutable
    {
      if (ec)
      {
        cb ? cb(ec, 0) : void();
      }
      else
      {
        std::error_code err;
        const size_t discardBufferSize = 8;
        std::size_t bytesToRead = bufSize - putPosition;
        if (bytesToRead > discardBufferSize)
          bytesToRead = discardBufferSize;

        const std::size_t bytesActuallyRead = sock.receive(asio::buffer(buf + putPosition, bytesToRead), asio::ip::tcp::socket::message_peek, err);
        if (err)
        {
          cb ? cb(ec, 0) : void();
        }
        else if (!bytesActuallyRead)
        {
          std::cout << "Zero Bytes Received: " __FILE__ << "/" << __LINE__ << std::endl;
        }
        else
        {
          bufEnd += bytesActuallyRead;

          bool delimFound = false;
          char* searchResult = std::search(buf, bufEnd, delim.begin(), delim.end());
          if (searchResult != bufEnd)
          {
            bufEnd = searchResult + delim.size();
            delimFound = true;
          }

          std::array<char, discardBufferSize> discard;
          size_t bytesToDiscard = bufEnd - &buf[putPosition];
          sock.receive(asio::buffer(discard.data(), bytesToDiscard), 0, err);
          putPosition += bytesToDiscard;

          std::size_t bufOutputSize = bufEnd - buf;
          if (delimFound || err)
            cb ? cb(err, bufOutputSize) : void();
          else if (bufOutputSize == bufSize)
            cb ? cb(make_error_code(std::errc::value_too_large), bufOutputSize) : void();
          else
            tcp::recvline(sock, buf, bufSize, putPosition, bufEnd, cb, delim);
        }
      }
    });
  }
  //----------------------------------------------------------------//

  //----------------------------------------------------------------//
  void tcp::recvline(asio::ip::tcp::socket& sock, char* buf, std::size_t bufSize, const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& cb, const std::string& delim)
  {
    if (!bufSize)
      cb ? cb(make_error_code(std::errc::value_too_large), 0) : void();
    else
      tcp::recvline(sock, buf, bufSize, 0, buf, cb, delim);
  };
  //----------------------------------------------------------------//

  //----------------------------------------------------------------//
  void tcp::recvline(asio::ssl::stream<asio::ip::tcp::socket>& ssl_stream, char* buf, std::size_t bufSize, std::size_t putPosition, char* bufEnd,
                     const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& cb,
                     const std::string& delim)
  {
    asio::async_read(ssl_stream, asio::null_buffers(), [&ssl_stream, buf, bufSize, putPosition, bufEnd, delim, cb](const std::error_code& ec, std::size_t bytes_transferred) mutable
    {
      if (ec)
      {
        cb ? cb(ec, 0) : void();
      }
      else
      {
        std::error_code err;
        const size_t discardBufferSize = 8;
        std::size_t bytesToRead = bufSize - putPosition;
        if (bytesToRead > discardBufferSize)
          bytesToRead = discardBufferSize;

        //const std::size_t bytesActuallyRead = sock.receive(asio::buffer(buf + putPosition, bytesToRead), asio::ip::tcp::socket::message_peek, err);
        int bytesActuallyRead = SSL_peek(ssl_stream.native_handle(), buf + putPosition, bytesToRead);
        if (SSL_get_error(ssl_stream.native_handle(), bytesActuallyRead))
        {
          cb ? cb(asio::error::make_error_code(asio::error::ssl_errors()), 0) : void();
        }
        else if (!bytesActuallyRead)
        {
          std::cout << "Zero Bytes Received: " __FILE__ << "/" << __LINE__ << std::endl;
        }
        else
        {
          bufEnd += bytesActuallyRead;

          bool delimFound = false;
          char* searchResult = std::search(buf, bufEnd, delim.begin(), delim.end());
          if (searchResult != bufEnd)
          {
            bufEnd = searchResult + delim.size();
            delimFound = true;
          }

          std::array<char, discardBufferSize> discard;
          size_t bytesToDiscard = bufEnd - &buf[putPosition];
          asio::read(ssl_stream, asio::buffer(discard.data(), bytesToDiscard), err);
          putPosition += bytesToDiscard;

          std::size_t bufOutputSize = bufEnd - buf;
          if (delimFound || err)
            cb ? cb(err, bufOutputSize) : void();
          else if (bufOutputSize == bufSize)
            cb ? cb(make_error_code(std::errc::value_too_large), bufOutputSize) : void();
          else
            tcp::recvline(ssl_stream, buf, bufSize, putPosition, bufEnd, cb, delim);
        }
      }
    });
  }
  //----------------------------------------------------------------//

  //----------------------------------------------------------------//
  void tcp::recvline(asio::ssl::stream<asio::ip::tcp::socket>& ssl_stream, char* buf, std::size_t bufSize, const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& cb, const std::string& delim)
  {
    if (!bufSize)
      cb ? cb(make_error_code(std::errc::value_too_large), 0) : void();
    else
      tcp::recvline(ssl_stream, buf, bufSize, 0, buf, cb, delim);
  };
  //----------------------------------------------------------------//

//  //----------------------------------------------------------------//
//  bool TCP::sendAll(Socket& sock, const char* buf, std::size_t bufSize)
//  {
//    bool ret = false;
//    std::size_t localBytesSent = 0;
//    while (localBytesSent < bufSize)
//    {
//      ssize_t sendResult = sock.send(&buf[localBytesSent], bufSize - localBytesSent);
//      if (sendResult < 0)
//      {
//        break;
//      }
//      else
//      {
//        localBytesSent += sendResult;
//      }
//    }
//
//    if (localBytesSent == bufSize)
//      ret = true;
//    return ret;
//  }
//  //----------------------------------------------------------------//
}
//################################################################//