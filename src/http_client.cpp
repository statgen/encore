
#include "http_client.hpp"
#include "uniform_resource_identifier.hpp"

#include <map>
#include <set>
#include <memory>

namespace manifold
{
  namespace http
  {
    bool verify_certificate(bool preverified,
      asio::ssl::verify_context& ctx)
    {
      // The verify callback can be used to check whether the certificate that is
      // being presented is valid for the peer. For example, RFC 2818 describes
      // the steps involved in doing this for HTTPS. Consult the OpenSSL
      // documentation for more details. Note that the callback is called once
      // for each certificate in the certificate chain, starting from the root
      // certificate authority.

      // In this example we will simply print the certificate's subject name.
      std::cout << "HERE I AM!!!!!!!!!!!!!!!" << std::endl;
      char subject_name[256];
      X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
      X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
      std::cout << "Verifying " << subject_name << "\n";

      return preverified;
    }

    class endpoint
    {
    public:
      endpoint() {}
//      endpoint(const uri& uri)
//        : host_(uri.host()), port_(uri.port()), encrypted_(uri.scheme_name() == "https")
//      {
//        if (!port_)
//          port_ = (unsigned short)(encrypted_ ? 443 : 80);
//      }
      endpoint(bool encrypted, const std::string& host, std::uint16_t port = 0)
        : host_(host), port_(port), encrypted_(encrypted)
      {
        if (!port_)
          port_ = (unsigned short)(encrypted_ ? 443 : 80);
      }

      bool operator==(const endpoint& other) const
      {
        return (this->host_ == other.host_ && this->port_ == other.port_ && this->encrypted_ == other.encrypted_);
      }

      const std::string& host() const { return host_; }
      unsigned short port() const { return port_; }
      bool encrypted() const { return encrypted_; }
      std::string socket_address() const
      {
        std::stringstream ret;
        ret << this->host_ << ":" << this->port_;
        return ret.str();
      }
    private:
      std::string host_;
      std::uint16_t port_;
      bool encrypted_;
    };
  }
}

namespace std
{
  size_t hash<manifold::http::endpoint>::operator()(const manifold::http::endpoint& ep) const
  {
    size_t result = hash<std::string>()(ep.host());
    result = result ^ hash<std::uint16_t>()(ep.port());
    return result ^ (std::size_t)ep.encrypted();
  }
}

namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    client::request::request(request_head&& head, const std::shared_ptr<http::connection<request_head, response_head>>& conn, std::uint32_t stream_id, const std::string& server_authority)
      : outgoing_message(conn, stream_id), head_(std::move(head)), server_authority_(server_authority)
    {
      if (this->connection_)
      {
        conn->on_push_promise(stream_id, [conn](request_head&& req, std::uint32_t promised_stream_id)
        {
          conn->send_reset_stream(promised_stream_id, v2_errc::refused_stream);
        });
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::request::request(request&& source)
      : outgoing_message(std::move(source)), head_(std::move(source.head_)), server_authority_(std::move(source.server_authority_))
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::request::~request()
    {
      if (this->connection_)
        this->end();
      std::cout << "client::request::~request()" << std::endl;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    request_head& client::request::head()
    {
      return this->head_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool client::request::send_headers(bool end_stream)
    {
      if (this->head_.method() == "GET" || this->head_.method() == "HEAD")
        end_stream = true;
      if (this->head().authority().empty())
        this->head().authority(this->server_authority_);
      return outgoing_message::send_headers(end_stream);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::request::on_push_promise(const std::function<void(http::client::request&& request)>& cb)
    {
      if (this->connection_)
      {
        auto c = this->connection_;
        std::uint32_t stream_id = this->stream_id_;
        this->connection_->on_push_promise(this->stream_id_, [cb, c, stream_id](request_head&& headers, std::uint32_t promised_stream_id)
        {
          std::string authority = headers.authority();
          cb ? cb(http::client::request(std::move(headers), c, promised_stream_id, authority)) : void();
        });
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::request::on_informational_headers(const std::function<void(response_head&& resp_head)>& cb)
    {
      if (this->connection_)
        this->connection_->on_informational_headers(this->stream_id_, cb) ;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::request::on_response(const std::function<void(http::client::response && resp)>& cb)
    {
      if (this->connection_)
      {
        auto c = this->connection_;
        std::uint32_t stream_id = this->stream_id_;
        this->connection_->on_headers(this->stream_id_, [cb, c, stream_id](response_head&& headers)
        {
          cb ? cb(http::client::response(std::move(headers), c, stream_id)) : void();
        });
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::response::response(response_head&& head, const std::shared_ptr<http::connection<request_head, response_head>>& conn, std::uint32_t stream_id)
      : incoming_message(conn, stream_id)
    {
      this->head_ = std::move(head);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::response::response(response&& source)
      : incoming_message(std::move(source)), head_(std::move(source.head_))
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::response::~response()
    {
      std::cout << "client::response::~response()" << std::endl;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const response_head& client::response::head() const
    {
      return this->head_;
    }
    //----------------------------------------------------------------//

    //================================================================//
    class session_base
    {
    public:
      session_base(const endpoint& ep, std::chrono::system_clock::duration timeout)
        : ep_(ep), timeout_(timeout) { }

      virtual ~session_base()
      {
      }

      virtual void close_socket() = 0;

      bool is_good() const
      {
        return !closed_;
      }

      void close(const std::error_code& ec)
      {
        if (!this->closed_)
        {
          this->closed_ = true;

          this->process_deferred_requests(ec);

          if (this->conn_)
            this->conn_->close(v2_errc::cancel); // TODO: switch to error_code.
          else
            this->close_socket();

          this->on_close_ ? this->on_close_(ec) : void();
          this->on_close_ = nullptr;
        }
      }

      void on_close(const std::function<void(const std::error_code& ec)>& fn) { this->on_close_ = fn; }

      void push_request(const std::function<void(const std::error_code& connect_error, client::request&& req)>& handler)
      {
        if (this->is_good())
        {
          if (this->conn_)
          {
            std::uint32_t stream_id = this->conn_->create_stream(0, 0); // TODO: handle error.
            handler ? handler(std::error_code(), client::request(request_head("/", "GET", {{"user-agent", "Manifold"}}), this->conn_, stream_id, this->ep_.socket_address())) : void();
          }
          else
          {
            this->deferred_requests_.emplace(handler);
          }
        }
      }

      void process_deferred_requests(const std::error_code& ec)
      {
        if (!this->deferred_requests_processed_)
        {
          this->deferred_requests_processed_ = true;

          while (this->deferred_requests_.size())
          {
            if (this->deferred_requests_.front())
            {
              if (ec || !this->conn_ || this->conn_->is_closed())
              {
                this->deferred_requests_.front()(ec ? ec : std::make_error_code(std::errc::connection_aborted), client::request(request_head(), nullptr, 0, ""));
              }
              else
              {
                std::uint32_t stream_id = this->conn_->create_stream(0, 0);
                this->deferred_requests_.front()(std::error_code(), client::request(request_head("/", "GET", {{"user-agent", "Manifold"}}), this->conn_, stream_id, this->ep_.socket_address()));
              }
            }
            this->deferred_requests_.pop();
          }
        }
      }

      const endpoint& ep() const { return this->ep_; }
    protected:
      endpoint ep_;
      std::chrono::system_clock::duration timeout_;
      std::shared_ptr<http::connection<request_head, response_head>> conn_;
      std::queue<std::function<void(const std::error_code& connect_error, client::request&& req)>> deferred_requests_;
      bool deferred_requests_processed_ = false;
      std::function<void(const std::error_code& ec)> on_close_;
      bool closed_ = false;
    };
    //================================================================//

    //================================================================//
    class non_tls_session : public session_base, public std::enable_shared_from_this<non_tls_session>
    {
    public:
      non_tls_session(asio::io_service& ioservice, const endpoint& ep, std::chrono::system_clock::duration timeout)
        : session_base(ep, timeout), sock_(ioservice)
      {
      }

      ~non_tls_session()
      {
        this->close(std::make_error_code(std::errc::operation_canceled));
      }

      void handle_resolve(const std::error_code& ec, asio::ip::tcp::resolver::iterator it)
      {
        if (it == asio::ip::tcp::resolver::iterator())
        {
          assert(ec);
          this->close(ec);
        }
        else if (!this->closed_)
        {
          std::cout << it->host_name() << std::endl;
          std::cout << it->endpoint().address().to_string() << std::endl;
          std::cout << it->endpoint().port() << std::endl;

          auto endpoint_to_try = it++;
          auto self = shared_from_this();
          ((asio::ip::tcp::socket&)this->sock_).async_connect(*endpoint_to_try, [self, it](const std::error_code& ec)
          {
            if (!self->closed_)
            {
              if (ec)
              {
                if (it != asio::ip::tcp::resolver::iterator())
                  self->sock_.reset();
                self->handle_resolve(ec, it);
              }
              else
              {
                auto c = std::make_shared<v1_connection<request_head, response_head>>(std::move(self->sock_));
                self->conn_ = c;
                c->on_close(std::bind(&session_base::close, self, std::placeholders::_1));
                c->run(self->timeout_);
                self->process_deferred_requests(std::error_code());
              }
            }
          });
        }
      }

      void close_socket()
      {
        this->sock_.close();
      }
    private:
      non_tls_socket sock_;
    };
    //================================================================//

    //================================================================//
    class tls_session : public session_base, public std::enable_shared_from_this<tls_session>
    {
    public:
      tls_session(asio::io_service& ioservice, asio::ssl::context& ctx, const endpoint& ep, std::chrono::system_clock::duration timeout)
        : session_base(ep, timeout), sock_(ioservice, ctx)
      {
      }

      ~tls_session()
      {
        this->close(std::make_error_code(std::errc::operation_canceled));
      }

      void handle_resolve(const std::error_code& ec, asio::ip::tcp::resolver::iterator it)
      {
        if (it == asio::ip::tcp::resolver::iterator())
        {
          assert(ec);
          this->close(ec);
        }
        else if (!this->closed_)
        {
          std::cout << it->host_name() << std::endl;
          std::cout << it->endpoint().address().to_string() << std::endl;
          std::cout << it->endpoint().port() << std::endl;

          auto endpoint_to_try = it++;
          auto self = shared_from_this();
          ((asio::ssl::stream<asio::ip::tcp::socket>&)this->sock_).next_layer().async_connect(*endpoint_to_try, [self, it](const std::error_code& ec)
          {
            if (!self->closed_)
            {
              if (ec)
              {
                if (it != asio::ip::tcp::resolver::iterator())
                  self->sock_.reset();
                self->handle_resolve(ec, it);
              }
              else
              {
                ((asio::ssl::stream<asio::ip::tcp::socket>&) self->sock_).async_handshake(asio::ssl::stream_base::client, [self](const std::error_code& ec)
                {
                  const unsigned char* selected_alpn = nullptr;
                  unsigned int selected_alpn_sz = 0;
                  SSL_get0_alpn_selected(((asio::ssl::stream<asio::ip::tcp::socket>&) self->sock_).native_handle(), &selected_alpn, &selected_alpn_sz);
                  std::cout << "Client ALPN: " << std::string((char*) selected_alpn, selected_alpn_sz) << std::endl;
                  if (ec)
                  {
                    std::cout << "ERROR: " << ec.message() << std::endl;
                    self->close(ec);
                  }
#ifndef MANIFOLD_DISABLE_HTTP2
                  else if (!self->closed_)
                  {
                    if (std::string((char*) selected_alpn, selected_alpn_sz) == "h2")
                    {
                      self->sock_.send(http::v2_connection<request_head, response_head>::preface.data(), http::v2_connection<request_head, response_head>::preface.size(), [self](const std::error_code& ec, std::size_t bytes_transfered)
                      {
                        if (ec)
                        {
                          self->close(ec);
                        }
                        else if (!self->closed_)
                        {
                          auto c = std::make_shared<v2_connection<request_head, response_head>>(std::move(self->sock_));
                          self->conn_ = c;
                          c->on_close(std::bind(&session_base::close, self, std::placeholders::_1));
                          c->run(self->timeout_, {{v2_connection<request_head, response_head>::setting_code::initial_window_size, 0x7FFFFFFF}});
                          self->process_deferred_requests(std::error_code());
                        }
                      });
                    }
#endif //MANIFOLD_DISABLE_HTTP2
                    else
                    {
                      auto c = std::make_shared<v1_connection<request_head, response_head>>(std::move(self->sock_));
                      self->conn_ = c;
                      c->on_close(std::bind(&session_base::close, self, std::placeholders::_1));
                      c->run(self->timeout_);
                      self->process_deferred_requests(std::error_code());
                    }
                  }
                });
              }
            }
          });
        }
      }

      void close_socket()
      {
        this->sock_.close();
      }
    private:
      tls_socket sock_;
    };
    //================================================================//

    //----------------------------------------------------------------//
    client::client(asio::io_service& ioservice, asio::ssl::context& ctx)
      : io_service_(ioservice), ssl_ctx_(ctx), tcp_resolver_(ioservice)
    {
      this->reset_timeout();
      std::vector<unsigned char> proto_list(::strlen(MANIFOLD_HTTP_ALPN_SUPPORTED_PROTOCOLS));
      std::copy_n(MANIFOLD_HTTP_ALPN_SUPPORTED_PROTOCOLS, ::strlen(MANIFOLD_HTTP_ALPN_SUPPORTED_PROTOCOLS), proto_list.begin());
      ::SSL_CTX_set_alpn_protos(this->ssl_ctx_.impl(), proto_list.data(), proto_list.size());
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::~client()
    {
      this->shutdown();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::reset_timeout(std::chrono::system_clock::duration value)
    {
      this->timeout_ = value;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::shutdown()
    {
      std::unordered_multimap<endpoint,std::shared_ptr<non_tls_session>> non_tls_tmp;
      this->non_tls_sessions_.swap(non_tls_tmp);
      for (auto it = non_tls_tmp.begin(); it != non_tls_tmp.end(); ++it)
        it->second->close(std::make_error_code(std::errc::operation_canceled));

      std::unordered_multimap<endpoint,std::shared_ptr<tls_session>> tls_tmp;
      this->tls_sessions_.swap(tls_tmp);
      for (auto it = tls_tmp.begin(); it != tls_tmp.end(); ++it)
        it->second->close(std::make_error_code(std::errc::operation_canceled));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::make_request(const std::string& host, std::uint16_t port, const std::function<void(const std::error_code& connect_error, client::request&& req)>& cb)
    {
      if (port == 0)
        port = 80;

      endpoint ep(false, host, port);

      auto it = this->non_tls_sessions_.end();
      for (auto range = this->non_tls_sessions_.equal_range(ep); range.first != range.second; ++(range.first))
      {
        if (range.first->second->is_good())
          it = range.first;
      }

      if (it != this->non_tls_sessions_.end())
      {
        it->second->push_request(cb);
      }
      else
      {
        auto sess = (this->non_tls_sessions_.emplace(ep, std::make_shared<non_tls_session>(this->io_service_, ep, this->timeout_)))->second;
        sess->on_close(std::bind(&client::handle_non_tls_session_close, this, std::placeholders::_1, sess));
        sess->push_request(cb);

        this->tcp_resolver_.async_resolve(asio::ip::tcp::resolver::query(ep.host(), std::to_string(ep.port())), std::bind(&non_tls_session::handle_resolve, sess, std::placeholders::_1, std::placeholders::_2));
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::make_secure_request(const std::string& host, std::uint16_t port, const std::function<void(const std::error_code& connect_error, client::request&& req)>& cb)
    {
      if (port == 0)
        port = 443;

      endpoint ep(false, host, port);

      auto it = this->tls_sessions_.end();
      for (auto range = this->tls_sessions_.equal_range(ep); range.first != range.second; ++(range.first))
      {
        if (range.first->second->is_good())
          it = range.first;
      }

      if (it != this->tls_sessions_.end())
      {
        it->second->push_request(cb);
      }
      else
      {
        auto sess = (this->tls_sessions_.emplace(ep, std::make_shared<tls_session>(this->io_service_, this->ssl_ctx_, ep, this->timeout_)))->second;
        sess->on_close(std::bind(&client::handle_tls_session_close, this, std::placeholders::_1, sess));
        sess->push_request(cb);

        this->tcp_resolver_.async_resolve(asio::ip::tcp::resolver::query(ep.host(), std::to_string(ep.port())), std::bind(&tls_session::handle_resolve, sess, std::placeholders::_1, std::placeholders::_2));
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::handle_non_tls_session_close(const std::error_code& ec, const std::shared_ptr<non_tls_session>& sess)
    {
      for (auto range = this->non_tls_sessions_.equal_range(sess->ep()); range.first != range.second; ++range.first)
      {
        if (range.first->second == sess)
        {
          this->non_tls_sessions_.erase(range.first);
          break;
        }
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::handle_tls_session_close(const std::error_code& ec, const std::shared_ptr<tls_session>& sess)
    {
      for (auto range = this->tls_sessions_.equal_range(sess->ep()); range.first != range.second; ++range.first)
      {
        if (range.first->second == sess)
        {
          this->tls_sessions_.erase(range.first);
          break;
        }
      }
    }
    //----------------------------------------------------------------//


#if 0 // OLD CLIENT
    class client_impl : public std::enable_shared_from_this<client_impl>
    {
    private:
      asio::io_service& io_service_;
      asio::ip::tcp::resolver tcp_resolver_;
      std::string default_user_agent_ = "Manifold";
      std::string socket_address_;

      std::unique_ptr<asio::ssl::context> ssl_context_;
      std::shared_ptr<http::connection<request_head, response_head>> connection_;
      // std::queue<std::pair<client::request, std::function<void(http::client::request && req)>>> pending_requests_;

      std::function<void()> on_connect_;
      std::function<void(errc ec)> on_close_;
      errc ec_;
      bool closed_ = false;

      void destroy_callbacks_later()
      {
        auto self = shared_from_this();
        this->io_service_.post([self]()
        {
          self->on_connect_ = nullptr;
          self->on_close_ = nullptr;
        });
      }
      //void send_connection_preface(std::function<void(const std::error_code& ec)>& fn);
    public:
      client_impl(asio::io_service& ioservice)
        : io_service_(ioservice), tcp_resolver_(ioservice)
      {

      }

      ~client_impl()
      {
        this->close(errc::cancel);
      }

      void connect(const std::string& host, unsigned short port)
      {
        if (!port)
          port = 80;

        if (true) //port != 80)
          this->socket_address_ = host + ":" + std::to_string(port);
        else
          this->socket_address_ = host;


        auto self = shared_from_this();
        this->tcp_resolver_.async_resolve(asio::ip::tcp::resolver::query(host, std::to_string(port)), std::bind(&client_impl::handle_resolve, self, std::placeholders::_1, std::placeholders::_2));
      }

      void connect(const std::string& host, const client::ssl_options& options, unsigned short port)
      {
        this->ssl_context_ = std::unique_ptr<asio::ssl::context>(new asio::ssl::context(options.method));

        if (!port)
          port = 443;

        if (true) //port != 443)
          this->socket_address_ = host + ":" + std::to_string(port);
        else
          this->socket_address_ = host;

        this->ssl_context_->set_default_verify_paths();

        auto self = shared_from_this();
        this->tcp_resolver_.async_resolve(asio::ip::tcp::resolver::query(host, std::to_string(port)), std::bind(&client_impl::handle_resolve_tls, self, std::placeholders::_1, std::placeholders::_2));
      }

      void handle_resolve(const std::error_code& ec, asio::ip::tcp::resolver::iterator it)
      {
        if (it == asio::ip::tcp::resolver::iterator())
        {
          this->ec_ = errc::internal_error;
          this->on_close_ ? this->on_close_(this->ec_) : void();
        }
        else
        {
          std::cout << it->host_name() << std::endl;
          std::cout << it->endpoint().address().to_string() << std::endl;
          std::cout << it->endpoint().port() << std::endl;

          auto sock = std::make_shared<manifold::non_tls_socket>(this->io_service_);

          auto self = shared_from_this();
          ((asio::ip::tcp::socket&)*sock).async_connect(*it, [self, sock, it](const std::error_code& ec)
          {
            if (ec)
            {
              self->handle_resolve(ec, ++asio::ip::tcp::resolver::iterator(it));
            }
            else
            {
              self->connection_ = std::make_shared<v1_connection<request_head, response_head>>(std::move(*sock));
              self->connection_->on_close(std::bind(&client_impl::close, self, std::placeholders::_1));
              self->connection_->run();
              self->on_connect_ ? self->on_connect_() : void();
            }
          });
        }
      }

      void handle_resolve_tls(const std::error_code& ec, asio::ip::tcp::resolver::iterator it)
      {
        if (it == asio::ip::tcp::resolver::iterator())
        {
          this->ec_ = errc::internal_error;
          this->on_close_ ? this->on_close_(this->ec_) : void();
        }
        else
        {
          std::cout << it->host_name() << std::endl;
          std::cout << it->endpoint().address().to_string() << std::endl;
          std::cout << it->endpoint().port() << std::endl;

          std::vector<unsigned char> proto_list(::strlen(MANIFOLD_HTTP_ALPN_SUPPORTED_PROTOCOLS));
          std::copy_n(MANIFOLD_HTTP_ALPN_SUPPORTED_PROTOCOLS, ::strlen(MANIFOLD_HTTP_ALPN_SUPPORTED_PROTOCOLS), proto_list.begin());
          //SSL_CTX_set_alpn_select_cb(this->ssl_context_->impl(), client_alpn_select_proto_cb, nullptr);
          const unsigned char* test = this->ssl_context_->impl()->alpn_client_proto_list;
          auto r = SSL_CTX_set_alpn_protos(this->ssl_context_->impl(), proto_list.data(), proto_list.size());
          const unsigned char* test2 = this->ssl_context_->impl()->alpn_client_proto_list;
          auto sock = std::make_shared<manifold::tls_socket>(this->io_service_, *this->ssl_context_);
          std::error_code e;
          ((asio::ssl::stream<asio::ip::tcp::socket>&)*sock).set_verify_mode(asio::ssl::verify_none, e);
          //((asio::ssl::stream<asio::ip::tcp::socket>&)*sock).set_verify_callback(verify_certificate, e);

          auto self = shared_from_this();
          ((asio::ssl::stream<asio::ip::tcp::socket>&)*sock).next_layer().async_connect(*it, [self, sock, it](const std::error_code& ec)
          {
            if (ec)
            {
              self->handle_resolve_tls(ec, ++asio::ip::tcp::resolver::iterator(it));
            }
            else
            {
              ((asio::ssl::stream<asio::ip::tcp::socket>&)*sock).async_handshake(asio::ssl::stream_base::client, [self, sock](const std::error_code& ec)
              {
                const unsigned char* selected_alpn = nullptr;
                unsigned int selected_alpn_sz = 0;
                SSL_get0_alpn_selected(((asio::ssl::stream<asio::ip::tcp::socket>&)*sock).native_handle(), &selected_alpn, &selected_alpn_sz);
                std::cout << "Client ALPN: " << std::string((char*)selected_alpn, selected_alpn_sz) << std::endl;
                if (ec)
                {
                  std::cout << "ERROR: " << ec.message() << std::endl;
                  self->ec_ = errc::internal_error;
                  self->on_close_ ? self->on_close_(self->ec_) : void();
                }
#ifndef MANIFOLD_DISABLE_HTTP2
                  else if (std::string((char*)selected_alpn, selected_alpn_sz) == "h2")
                {
                  sock->send(http::v2_connection<request_head, response_head>::preface.data(), http::v2_connection<request_head, response_head>::preface.size(), [self, sock](const std::error_code& ec, std::size_t bytes_transfered)
                  {
                    if (ec)
                    {
                      self->ec_ = errc::internal_error;
                      self->on_close_ ? self->on_close_(self->ec_) : void();
                    }
                    else
                    {
                      self->connection_ = std::make_shared<http::v2_connection<request_head, response_head>>(std::move(*sock));
                      self->connection_->on_close([self](errc ec) { self->on_close_ ? self->on_close_(ec) : void(); });
                      self->connection_->run();
                      self->on_connect_ ? self->on_connect_() : void();
                    }
                  });
                }
#endif //MANIFOLD_DISABLE_HTTP2
                else
                {
                  self->connection_ = std::make_shared<http::v1_connection<request_head, response_head>>(std::move(*sock));
                  self->connection_->on_close([self](errc ec) { self->on_close_ ? self->on_close_(ec) : void(); });
                  self->connection_->run();
                  self->on_connect_ ? self->on_connect_() : void();
                }
              });
            }
          });
        }
      }

      void on_connect(const std::function<void()>& fn)
      {
        if (this->connection_)
          fn ? fn() : void();
        else
          this->on_connect_ = fn;
      }

      void on_close(const std::function<void(errc ec)>& fn)
      {
        if (this->closed_)
          fn ? fn(this->ec_) : void();
        else
          this->on_close_ = fn;
      }

      client::request make_request()
      {
        //TODO: this method needs better error handling.

        if (!this->connection_)
          throw std::invalid_argument("No connection.");


        std::uint32_t stream_id = this->connection_->create_stream(0, 0);

        return client::request(request_head("/", "GET", {{"user-agent", this->default_user_agent_}}), this->connection_, stream_id, this->socket_address_);
      }

      void close(errc ec)
      {
        if (!this->closed_)
        {
          this->closed_ = true;


          if (this->connection_)
          {
            this->connection_->close(ec);
          }

          this->on_close_ ? this->on_close_(ec) : void();

          this->destroy_callbacks_later();
        }
      }

      void set_default_user_agent(const std::string user_agent)
      {
        this->default_user_agent_ = user_agent;
      }
    };

//    int client_alpn_select_proto_cb(SSL *ssl, const unsigned char **out,
//      unsigned char *out_len, const unsigned char *in,
//      unsigned int in_len, void *arg)
//    {
//      static const char*const h2_proto_string = "\x02h2";
//      std::size_t h2_proto_string_len = ::strlen(h2_proto_string);
//      const unsigned char* client_proto = in;
//      const unsigned char* client_proto_end = in + in_len;
//      for ( ; client_proto + h2_proto_string_len <= client_proto_end; client_proto += *client_proto + 1)
//      {
//        std::size_t client_proto_len = (*client_proto + 1);
//        if (::memcmp(h2_proto_string, client_proto, h2_proto_string_len <  client_proto_len ? h2_proto_string_len : client_proto_len) == 0)
//        {
//          *out = client_proto;
//          *out_len = (unsigned char)client_proto_len;
//          return SSL_TLSEXT_ERR_OK;
//        }
//      }
//      return SSL_TLSEXT_ERR_NOACK;
//    }

//    //----------------------------------------------------------------//
//    void client::v2_connection::on_informational_headers(std::uint32_t stream_id, const std::function<void(v2_response_head&& headers)>& fn)
//    {
//      auto it = this->streams_.find(stream_id);
//      if (it == this->streams_.end())
//      {
//        // TODO: Handle error
//      }
//      else
//      {
//        ((stream*)it->second.get())->on_informational_headers(fn);
//      }
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    void client::v2_connection::on_response(std::uint32_t stream_id, const std::function<void(http::client::v2_response && resp)>& fn)
//    {
//      auto self = shared_from_this();
//      auto it = this->streams_.find(stream_id);
//      if (it == this->streams_.end())
//      {
//        // TODO: Handle error
//      }
//      else
//      {
//        ((stream*)it->second.get())->on_response_headers([self, fn, stream_id](v2_response_head&& headers)
//        {
//          fn(http::client::response(std::move(headers), self, stream_id));
//        });
//      }
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    void client::v2_connection::on_trailers(std::uint32_t stream_id, const std::function<void(v2_header_block&& headers)>& fn)
//    {
//      auto it = this->streams_.find(stream_id);
//      if (it == this->streams_.end())
//      {
//        // TODO: Handle error
//      }
//      else
//      {
//        ((stream*)it->second.get())->on_trailers(fn);
//      }
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    void client::v2_connection::on_push_promise(std::uint32_t stream_id, const std::function<void(http::client::v2_request && req)>& fn)
//    {
//      auto self = this->shared_from_this();
//      auto it = this->streams_.find(stream_id);
//      if (it == this->streams_.end())
//      {
//      // TODO: Handle error
//      }
//      else
//      {
//        it->second->on_push_promise([self, fn, stream_id](v2_request_head&& headers, std::uint32_t promised_stream_id)
//        {
//          fn(http::client::request(std::move(headers), self, promised_stream_id));
//        });
//      }
//    }

    //----------------------------------------------------------------//
    client::request::request(request_head&& head, const std::shared_ptr<http::connection<request_head, response_head>>& conn, std::uint32_t stream_id, const std::string& server_authority)
      : outgoing_message(conn, stream_id), head_(std::move(head)), server_authority_(server_authority)
    {
      if (this->connection_)
      {
        conn->on_push_promise(stream_id, [conn](request_head&& req, std::uint32_t promised_stream_id)
        {
          conn->send_reset_stream(promised_stream_id, errc::refused_stream);
        });
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::request::request(request&& source)
     : outgoing_message(std::move(source)), head_(std::move(source.head_)), server_authority_(std::move(source.server_authority_))
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::request::~request()
    {
      if (this->connection_)
        this->end();
      std::cout << "client::request::~request()" << std::endl;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    request_head& client::request::head()
    {
      return this->head_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool client::request::send_headers(bool end_stream)
    {
      if (this->head_.method() == "GET" || this->head_.method() == "HEAD")
        end_stream = true;
      if (this->head().authority().empty())
        this->head().authority(this->server_authority_);
      return outgoing_message::send_headers(end_stream);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::request::on_push_promise(const std::function<void(http::client::request&& request)>& cb)
    {
      if (this->connection_)
      {
        auto c = this->connection_;
        std::uint32_t stream_id = this->stream_id_;
        this->connection_->on_push_promise(this->stream_id_, [cb, c, stream_id](request_head&& headers, std::uint32_t promised_stream_id)
        {
          std::string authority = headers.authority();
          cb ? cb(http::client::request(std::move(headers), c, promised_stream_id, authority)) : void();
        });
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::request::on_informational_headers(const std::function<void(response_head&& resp_head)>& cb)
    {
      if (this->connection_)
        this->connection_->on_informational_headers(this->stream_id_, cb) ;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::request::on_response(const std::function<void(http::client::response && resp)>& cb)
    {
      if (this->connection_)
      {
        auto c = this->connection_;
        std::uint32_t stream_id = this->stream_id_;
        this->connection_->on_headers(this->stream_id_, [cb, c, stream_id](response_head&& headers)
        {
          cb ? cb(http::client::response(std::move(headers), c, stream_id)) : void();
        });
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::response::response(response_head&& head, const std::shared_ptr<http::connection<request_head, response_head>>& conn, std::uint32_t stream_id)
      : incoming_message(conn, stream_id)
    {
      this->head_ = std::move(head);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::response::response(response&& source)
      : incoming_message(std::move(source)), head_(std::move(source.head_))
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::response::~response()
    {
      std::cout << "client::response::~response()" << std::endl;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const response_head& client::response::head() const
    {
      return this->head_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::client(asio::io_service& ioservice, const std::string& host, unsigned short port)
      : impl_(std::make_shared<client_impl>(ioservice))
    {
      this->impl_->connect(host, port);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::client(asio::io_service& ioservice, const std::string& host, const ssl_options& options, unsigned short port)
        : impl_(std::make_shared<client_impl>(ioservice))
    {
      this->impl_->connect(host, options, port);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::client(client&& source)
    {
      this->impl_ = source.impl_;
      source.impl_ = nullptr;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    client::~client()
    {
      if (this->impl_)
        this->impl_->close(errc::cancel);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::close(errc ec)
    {
      this->impl_->close(ec);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    http::client::request client::make_request()
    {
      return this->impl_->make_request();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::on_connect(const std::function<void()>& fn)
    {
      this->impl_->on_connect(fn);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::on_close(const std::function<void(errc ec)>& fn)
    {
      this->impl_->on_close(fn);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void client::set_default_user_agent(const std::string user_agent)
    {
      this->impl_->set_default_user_agent(user_agent);
    }
    //----------------------------------------------------------------//
#endif
  }
}
