#pragma once
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#ifndef MANIFOLD_SOCKET_HPP
#define MANIFOLD_SOCKET_HPP



#include "asio.hpp"
#include "asio/ssl.hpp"

//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netdb.h>
//#include <cstdint>
//#include <string>
//#include <chrono>
//#include <sys/socket.h>

//################################################################//
namespace manifold
{
  //================================================================//
  class socket
  {
  public:
    socket() {}
    virtual ~socket() {}
    virtual asio::io_service& io_service() = 0;
    virtual void recv(char* buf, std::size_t buf_sz, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb) = 0;
    virtual void recv(char* buf, std::size_t buf_sz, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb) = 0;
    //virtual void recv(asio::streambuf& b, std::size_t bytes_to_recv, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb) = 0;
    //virtual void recv(asio::streambuf& b, std::size_t bytes_to_recv, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb) = 0;
    //virtual void recv_until(asio::streambuf& b, const std::string& delim, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb) = 0;
    //virtual void recv_until(asio::streambuf& b, const std::string& delim, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb) = 0;
    virtual void recvline(char* buf, std::size_t buf_sz, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb, const std::string& delim = "\r\n") = 0;
    virtual void recvline(char* buf, std::size_t buf_sz, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb, const std::string& delim = "\r\n") = 0;
    virtual void send(const char*const data, std::size_t data_sz, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb) = 0;
    virtual void send(const char*const data, std::size_t data_sz, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb) = 0;
    virtual void close() = 0;
    virtual void reset() = 0;
    virtual bool is_encrypted() const = 0;
  };
  //================================================================//

  //================================================================//
  class non_tls_socket : public socket
  {
  public:
    non_tls_socket(asio::io_service& ioservice)
      : s_(new asio::ip::tcp::socket(ioservice)) { }

    non_tls_socket(non_tls_socket&& source)
      : s_(source.s_)
    {
      source.s_ = nullptr;
    }

    ~non_tls_socket()
    {
      if (this->s_)
        delete this->s_;
    }

    void reset()
    {
      auto* new_sock = new asio::ip::tcp::socket(this->s_->get_io_service());
      delete this->s_;
      this->s_ = new_sock;
    }


    asio::io_service& io_service() { return this->s_->get_io_service(); }
    void recv(char* data, std::size_t data_sz, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb);
    void recv(char* data, std::size_t data_sz, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb);
    //void recv(asio::streambuf& b, std::size_t bytes_to_recv, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb);
    //void recv(asio::streambuf& b, std::size_t bytes_to_recv, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb);
    //void recv_until(asio::streambuf& b, const std::string& delim, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb);
    //void recv_until(asio::streambuf& b, const std::string& delim, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb);
    void recvline(char* buf, std::size_t buf_sz, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb, const std::string& delim = "\r\n");
    void recvline(char* buf, std::size_t buf_sz, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb, const std::string& delim = "\r\n");
    void send(const char*const data, std::size_t data_sz, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb);
    void send(const char*const data, std::size_t data_sz, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb);
    void close();
    bool is_encrypted() const { return false; }

    operator const asio::ip::tcp::socket& () const { return *this->s_; }
    operator asio::ip::tcp::socket& () { return *this->s_; }
  private:
    asio::ip::tcp::socket* s_;

    void recvline(char* buf, std::size_t bufSize, std::size_t putPosition, char* bufEnd, std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>&& cb, const std::string& delim);
  };
  //================================================================//

  //================================================================//
  class tls_socket : public socket
  {
  public:
    tls_socket(asio::io_service& ioservice, asio::ssl::context& ctx)
      : s_(new asio::ssl::stream<asio::ip::tcp::socket>(ioservice, ctx)), ssl_ctx_(ctx)
    {
      SSL_CTX_set_mode(ctx.native_handle(), SSL_MODE_AUTO_RETRY);
    }

    tls_socket(tls_socket&& source)
      : s_(source.s_), ssl_ctx_(source.ssl_ctx_)
    {
      source.s_ = nullptr;
    }

    ~tls_socket()
    {
      if (this->s_)
        delete this->s_;
    }

    void reset()
    {
      auto* new_sock = new asio::ssl::stream<asio::ip::tcp::socket>(this->s_->get_io_service(), this->ssl_ctx_);
      delete this->s_;
      this->s_ = new_sock;
    }

    asio::io_service& io_service() { return this->s_->get_io_service(); }
    void recv(char* data, std::size_t data_sz, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb);
    void recv(char* data, std::size_t data_sz, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb);
    //void recv(asio::streambuf& b, std::size_t bytes_to_recv, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb);
    //void recv(asio::streambuf& b, std::size_t bytes_to_recv, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb);
    //void recv_until(asio::streambuf& b, const std::string& delim, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb);
    //void recv_until(asio::streambuf& b, const std::string& delim, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb);
    void recvline(char* buf, std::size_t buf_sz, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb, const std::string& delim = "\r\n");
    void recvline(char* buf, std::size_t buf_sz, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb, const std::string& delim = "\r\n");
    void send(const char*const data, std::size_t data_sz, std::function<void(const std::error_code& ec, std::size_t bytes_read)>&& cb);
    void send(const char*const data, std::size_t data_sz, const std::function<void(const std::error_code& ec, std::size_t bytes_read)>& cb);
    void close();
    bool is_encrypted() const { return true; }

    operator const asio::ssl::stream<asio::ip::tcp::socket>& () const { return *this->s_; }
    operator asio::ssl::stream<asio::ip::tcp::socket>& () { return *this->s_; }
  private:
    asio::ssl::stream<asio::ip::tcp::socket>* s_;
    asio::ssl::context& ssl_ctx_;
    asio::streambuf recvline_buffer_;

    //void recvline(char* buf, std::size_t bufSize, std::size_t putPosition, char* bufEnd, std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>&& cb, const std::string& delim);
  };
  //================================================================//

//  //================================================================//
//  class Socket
//  {
//  public:
//    //----------------------------------------------------------------//
//    enum class Type
//    {
//      Unknown = 0,
//      Raw = SOCK_RAW,
//      Stream = SOCK_STREAM,
//      Datagram = SOCK_DGRAM,
//#ifdef SOCK_RDM
//      ReliablyDeliveredMessage = SOCK_RDM,
//#endif
//      SequencePacketStream = SOCK_SEQPACKET
//    };
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    enum class Family
//    {
//      Unspecified = AF_UNSPEC  , /* unspecified */
//      Unix = AF_UNIX           , /* local to host (pipes, portals) */
//      IPV4 = AF_INET           , /* internetwork: UDP, TCP, etc. */
//      IMPLink = AF_IMPLINK     , /* arpanet imp addresses */
//      Pup= AF_PUP              , /* pup protocols: e.g. BSP */
//      Chaos = AF_CHAOS         , /* mit CHAOS protocols */
//      XeroxNS = AF_NS          , /* XEROX NS protocols */
//      ISO = AF_ISO             , /* ISO protocols */
//      ECMA = AF_ECMA           , /* european computer manufacturers */
//      DataKit = AF_DATAKIT     , /* datakit protocols */
//      CCITT = AF_CCITT         , /* CCITT protocols, X.25 etc */
//      SNA = AF_SNA             , /* IBM SNA */
//      DECnet = AF_DECnet       , /* DECnet */
//      DLI = AF_DLI             , /* DEC Direct data link interface */
//      LAT = AF_LAT             , /* LAT */
//      HyLink = AF_HYLINK       , /* NSC Hyperchannel */
//      AppleTalk = AF_APPLETALK , /* Apple Talk */
//      Route = AF_ROUTE         , /* Internal Routing Protocol */
//      Link = AF_LINK           , /* Link layer interface */
//      IPV6 = AF_INET6          , /* ipv6 */
//      Max = AF_MAX
//    };
//    //----------------------------------------------------------------//
//  private:
//    //----------------------------------------------------------------//
//    Family family_;
//    Type type_;
//    int errno_;
//    int fd_;
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    Socket(const Socket& source) = delete;
//    Socket& operator=(const Socket& source) = delete;
//    //----------------------------------------------------------------//
//  public:
//    //----------------------------------------------------------------//
//    Socket();
//    Socket(Family fam, Type frame_type, int proto = 0);
//    Socket(Socket&& source);
//    Socket& operator=(Socket&& source);
//    virtual ~Socket();
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    int fd() const;
//    bool isValid() const;
//    //int errorCode() const;
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    bool connect(const struct sockaddr* addr, socklen_t len, std::int64_t microseconds = 0);
//    template<class Rep, class Period>
//    bool connect(const struct sockaddr* addr, socklen_t len, const std::chrono::duration<Rep,Period>& timeout = 0)
//    {
//      return this->connect(addr, len, std::chrono::duration_cast<std::chrono::microseconds>(timeout).count());
//    }
//    bool listen(int backlog);
//    bool bind(const struct sockaddr* addr, socklen_t addrlen);
//    Socket accept(struct sockaddr* addr, socklen_t* addrlen);
//    ssize_t send(const void *msg, std::size_t msglen, int flags = 0);
//    ssize_t sendto(const void *msg, std::size_t msglen, unsigned short port, const std::string& host);
//    ssize_t recv(void *buf, std::size_t len, int flags = 0);
//    ssize_t recvfrom(void *buf, std::size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
//    void close();
//    //----------------------------------------------------------------//
//  };
//  //================================================================//
}
//################################################################//

#endif // MANIFOLD_SOCKET_HPP