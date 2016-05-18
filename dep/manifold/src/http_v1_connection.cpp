
#include "http_v1_connection.hpp"
#include "http_v1_request_head.hpp"
#include "http_v1_response_head.hpp"

#include <sstream>

namespace manifold
{
  namespace http
  {
    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::on_close(const std::function<void(const std::error_code&)>& fn)
    {
      this->on_close_ = fn;
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::on_new_stream(const std::function<void(std::uint32_t)>& fn)
    {
      this->on_new_stream_ = fn;
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::on_data(std::uint32_t transaction_id, const std::function<void(const char* const, std::size_t)>& fn)
    {
      auto it = std::find_if(this->transaction_queue_.begin(), this->transaction_queue_.end(), [transaction_id](const transaction& t) { return (t.id == transaction_id); });
      if (it != this->transaction_queue_.end())
      {
        it->on_data = fn;
      }
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::on_headers(std::uint32_t transaction_id, const std::function<void(RecvMsg&&)>& fn)
    {
      auto it = std::find_if(this->transaction_queue_.begin(), this->transaction_queue_.end(), [transaction_id](const transaction& t) { return t.id == transaction_id; });
      if (it != this->transaction_queue_.end())
      {
        it->on_headers = fn;
      }
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::on_informational_headers(std::uint32_t transaction_id, const std::function<void(RecvMsg&&)>& fn)
    {
      auto it = std::find_if(this->transaction_queue_.begin(), this->transaction_queue_.end(), [transaction_id](const transaction& t) { return t.id == transaction_id; });
      if (it != this->transaction_queue_.end())
      {
        it->on_informational_headers = fn;
      }
    }
#ifndef MANIFOLD_REMOVED_TRAILERS
    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::on_trailers(std::uint32_t transaction_id, const std::function<void(header_block&&)>& fn)
    {
      auto it = std::find_if(this->transaction_queue_.begin(), this->transaction_queue_.end(), [transaction_id](const transaction& t) { return t.id == transaction_id; });
      if (it != this->transaction_queue_.end())
      {
        it->on_trailers = fn;
      }
    }
#endif
    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::on_close(std::uint32_t transaction_id, const std::function<void(const std::error_code&)>& fn)
    {
      auto it = std::find_if(this->transaction_queue_.begin(), this->transaction_queue_.end(), [transaction_id](const transaction& t) { return t.id == transaction_id; });
      if (it != this->transaction_queue_.end())
      {
        if (it->state == transaction_state::closed)
          fn ? fn(std::error_code()) : void();
        else
          it->on_close = fn;
      }
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::on_end(std::uint32_t transaction_id, const std::function<void()>& fn)
    {
      auto it = std::find_if(this->transaction_queue_.begin(), this->transaction_queue_.end(), [transaction_id](const transaction& t) { return t.id == transaction_id; });
      if (it != this->transaction_queue_.end())
      {
        if (it->state == transaction_state::closed || it->state == transaction_state::half_closed_remote)
          fn ? fn() : void();
        else
          it->on_end = fn;
      }
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::on_drain(std::uint32_t transaction_id, const std::function<void()>& fn)
    {
      auto it = std::find_if(this->transaction_queue_.begin(), this->transaction_queue_.end(), [transaction_id](const transaction& t) { return t.id == transaction_id; });
      if (it != this->transaction_queue_.end())
      {
        it->on_drain = fn;
      }
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::run(std::chrono::system_clock::duration timeout)
    {
      this->activity_timeout_ = timeout;
      this->activity_deadline_timer_.expires_from_now(this->activity_timeout_);
      this->run_timeout_loop(); // TODO: Test performance hit.
      this->run_recv_loop();
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::run_timeout_loop(const std::error_code& ec)
    {
      if (!this->is_closed())
      {
        auto self = casted_shared_from_this();
        if (self->activity_deadline_timer_.expires_from_now() <= std::chrono::system_clock::duration(0))
          this->close(http::errc::data_transfer_timeout); // TODO: check if idle.
        else
          this->activity_deadline_timer_.async_wait(std::bind(&v1_connection::run_timeout_loop, self, std::placeholders::_1));
      }
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::close(const std::error_code& ec)
    {
      if (!this->closed_)
      {
        this->closed_ = true;

        auto self = casted_shared_from_this();
        this->socket_->io_service().post([self, ec]()
        {
          self->socket_->close();
          for (auto it = self->transaction_queue_.begin(); it != self->transaction_queue_.end(); ++it)
          {
            if (it->state != transaction_state::closed)
            {
              it->state = transaction_state::closed;
              it->on_close ? it->on_close(ec) : void();
            }
          }

          self->on_close_ ? self->on_close_(ec) : void();

          self->activity_deadline_timer_.cancel();

          self->garbage_collect_transactions();
          self->on_close_ = nullptr;
          self->on_new_stream_ = nullptr;
        });
      }
    }

    template <typename SendMsg, typename RecvMsg>
    bool v1_connection<SendMsg, RecvMsg>::is_closed() const
    {
      return this->closed_;
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::garbage_collect_transactions()
    {
      while (this->transaction_queue_.size())
      {
        if (this->transaction_queue_.front().state == transaction_state::closed)
          this->transaction_queue_.pop_front();
        else
          break;
      }
    }

    template<> bool v1_connection<request_head, response_head>::incoming_head_is_head_request(const response_head& head) { return false; }
    template<> bool v1_connection<response_head, request_head>::incoming_head_is_head_request(const request_head& head) { return (head.method() == "HEAD"); }

    template<>
    bool v1_connection<request_head, response_head>::deserialize_incoming_headers(std::istream& is, response_head& generic_headers)
    {
      v1_response_head v1_headers;
      bool ret = v1_message_head::deserialize(is, v1_headers);
      generic_headers = response_head(std::move(v1_headers));
      return ret;
    }

    template<>
    bool v1_connection<response_head, request_head>::deserialize_incoming_headers(std::istream& is, request_head& generic_headers)
    {
      v1_request_head v1_headers;
      bool ret = v1_message_head::deserialize(is, v1_headers);
      generic_headers = request_head(std::move(v1_headers));
      return ret;
    }

    template <typename SendMsg, typename RecvMsg>
    typename v1_connection<SendMsg, RecvMsg>::transaction* v1_connection<SendMsg, RecvMsg>::current_send_transaction()
    {
      transaction* ret = nullptr;

      auto it = std::find_if(
        this->transaction_queue_.begin(),
        this->transaction_queue_.end(),
        [](const transaction& t) { return (t.state != transaction_state::closed && t.state != transaction_state::half_closed_local); });

      if (it != this->transaction_queue_.end())
        ret = &(*it);

      return ret;
    }

    template <typename SendMsg, typename RecvMsg>
    typename v1_connection<SendMsg, RecvMsg>::transaction* v1_connection<SendMsg, RecvMsg>::current_recv_transaction()
    {
      transaction* ret = nullptr;

      auto it = std::find_if(
        this->transaction_queue_.begin(),
        this->transaction_queue_.end(),
        [](const transaction& t) { return (t.state != transaction_state::closed && t.state != transaction_state::half_closed_remote); });

      if (it != this->transaction_queue_.end())
        ret = &(*it);

      return ret;
    }

    template <>
    void v1_connection<request_head, response_head>::run_recv_loop()
    {
      this->recv_headers();
    }

    template <>
    void v1_connection<response_head, request_head>::run_recv_loop()
    {
      std::uint32_t transaction_id = this->create_stream(0, 0);
      this->on_new_stream_ ? this->on_new_stream_(transaction_id) : void();

      this->recv_headers();
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::recv_headers()
    {
      this->activity_deadline_timer_.expires_from_now(this->activity_timeout_);
      auto self = casted_shared_from_this();
      this->socket_->recvline(this->recv_buffer_.data(), this->recv_buffer_.size(), [self](const std::error_code& ec, std::size_t bytes_read)
      {
        if (ec)
        {
          self->close(ec);
        }
        else if (!self->is_closed())
        {
          std::stringstream is(std::string(self->recv_buffer_.data(), bytes_read));

          RecvMsg headers;
          if (!v1_connection<SendMsg, RecvMsg>::deserialize_incoming_headers(is, headers))
          {
            self->close(errc::invalid_headers);
          }
          else
          {
            std::string transfer_encoding = headers.header("transfer-encoding");
            std::uint64_t content_length = 0;
            std::stringstream content_length_ss(headers.header("content-length"));
            content_length_ss >> content_length;

            transaction* current = self->current_recv_transaction();
            if (!current)
            {
              self->close(errc::unwarranted_response); //(server sending a response without a request).
            }
            else
            {
              if (incoming_head_is_head_request(headers))
                current->outgoing_ended = true;

              current->on_headers ? current->on_headers(std::move(headers)) : void();

              if (current->ignore_incoming_body) // TODO: || incoming_header_is_get_or_head_request(headers)
              {
                current->state = (current->state == transaction_state::half_closed_local ? transaction_state::closed : transaction_state::half_closed_remote);
                current->on_end ? current->on_end() : void();
                if (current->state == transaction_state::closed)
                {
                  if (current->on_close)
                    current->on_close(std::error_code()); // : void();
                  self->garbage_collect_transactions();
                }

                self->run_recv_loop();
              }
              else if (transfer_encoding.empty() || transfer_encoding == "identity")
              {
                self->recv_known_length_body(content_length);
              }
              else
              {
                self->recv_chunk_encoded_body();
              }
            }
          }
        }
      }, "\r\n\r\n");
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::recv_trailers()
    {
      auto self = casted_shared_from_this();
      this->socket_->recvline(this->recv_buffer_.data(), this->recv_buffer_.size(), [self](const std::error_code& ec, std::size_t bytes_read)
      {
        if (ec)
        {
          self->close(ec);
        }
        else if (!self->is_closed())
        {
          transaction* current = self->current_recv_transaction();

          if (!current)
          {
            assert(!"Should never happen.");
            self->close(errc::should_never_happen);
          }
          else
          {
            if (bytes_read > 2)
            {
              std::stringstream is(std::string(self->recv_buffer_.data(), bytes_read));
              v1_header_block::deserialize(is, current->incoming_trailers);
              self->recv_trailers();
            }
            else
            {
              if (current->incoming_trailers.size())
              {
                current->on_trailers ? current->on_trailers(std::move(current->incoming_trailers)) : void();
              }

              current->state = (current->state == transaction_state::half_closed_local ? transaction_state::closed : transaction_state::half_closed_remote);
              current->on_end ? current->on_end() : void();
              if (current->state == transaction_state::closed)
              {
                current->on_close ? current->on_close(std::error_code()) : void();
                self->garbage_collect_transactions();
              }

              self->run_recv_loop();
            }
          }
        }
      });
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::recv_chunk_encoded_body()
    {
      this->activity_deadline_timer_.expires_from_now(this->activity_timeout_);
      auto self = casted_shared_from_this();
      this->socket_->recvline(this->recv_buffer_.data(), this->recv_buffer_.size(), [self](const std::error_code& ec, std::size_t bytes_read)
      {
        if (ec)
        {
          self->close(ec);
        }
        else if (!self->is_closed())
        {
          // parse chunk size.
          std::string size_line(self->recv_buffer_.data(), bytes_read);
          std::size_t pos = size_line.find(';');
          std::string chunk_size_str = size_line.substr(0, pos);
          chunk_size_str.erase(0, chunk_size_str.find_first_not_of(" \r\n"));
          chunk_size_str.erase(chunk_size_str.find_last_not_of(" \r\n") + 1);

          char* not_converted = nullptr;
          unsigned long chunk_size = strtoul(chunk_size_str.c_str(), &not_converted, 16);
          if (*not_converted != '\0')
          {
            self->close(errc::chunked_encoding_corrupt);
          }
          else
          {
            if (chunk_size == 0)
            {
              self->recv_trailers();
            }
            else
            {
              self->recv_chunk_data(chunk_size);
            }
          }
        }
      });
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::recv_chunk_data(std::size_t chunk_size)
    {
      auto self = casted_shared_from_this();
      this->socket_->recv(self->recv_buffer_.data(), (chunk_size > this->recv_buffer_.size() ? this->recv_buffer_.size() : chunk_size), [self, chunk_size](const std::error_code& ec, std::size_t bytes_read)
      {
        if (ec)
        {
          self->close(ec);
        }
        else if (!self->is_closed())
        {
          transaction* current = self->current_recv_transaction();

          if (!current)
          {
            assert(!"Should never happen.");
            self->close(errc::should_never_happen);
          }
          else
          {
            current->on_data ? current->on_data(self->recv_buffer_.data(), bytes_read) : void();

            std::size_t bytes_remaining = chunk_size - bytes_read;

            if (bytes_remaining == 0)
            {
              self->socket_->recv(self->recv_buffer_.data(), 2, [self](const std::error_code &ec, std::size_t bytes_read)
              {
                if (ec)
                {
                  self->close(ec);
                }
                else if (!self->is_closed())
                {
                  self->recv_chunk_encoded_body();
                }
              });
            }
            else
            {
              self->recv_chunk_data(bytes_remaining);
            }
          }
        }
      });
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::recv_known_length_body(std::uint64_t content_length)
    {
      if (content_length == 0)
      {
        transaction* current = this->current_recv_transaction();
        if (!current)
        {
          assert(!"Should never happen.");
          this->close(errc::should_never_happen);
        }
        else
        {
          current->state = (current->state == transaction_state::half_closed_local ? transaction_state::closed : transaction_state::half_closed_remote);
          current->on_end ? current->on_end() : void();
          if (current->state == transaction_state::closed)
          {
            current->on_close ? current->on_close(std::error_code()) : void();
            this->garbage_collect_transactions();
          }

          this->run_recv_loop();
        }
      }
      else
      {
        this->activity_deadline_timer_.expires_from_now(this->activity_timeout_);
        std::size_t bytes_to_read = this->recv_buffer_.size();
        if (bytes_to_read > content_length)
          bytes_to_read = content_length;

        auto self = casted_shared_from_this();
        this->socket_->recv(this->recv_buffer_.data(), bytes_to_read, [self, content_length, bytes_to_read](const std::error_code& ec, std::size_t bytes_read)
        {
          if (ec)
          {
            self->close(ec);
          }
          else if (!self->is_closed())
          {
            transaction* current = self->current_recv_transaction();
            if (!current)
            {
              assert(!"Should never happen.");
              self->close(errc::should_never_happen);
            }
            else
            {
              current->on_data ? current->on_data(self->recv_buffer_.data(), bytes_to_read) : void();
              self->recv_known_length_body(content_length - bytes_to_read);
            }
          }
        });
      }
    }

    template <typename SendMsg, typename RecvMsg>
    void v1_connection<SendMsg, RecvMsg>::run_send_loop()
    {
      if (!this->send_loop_running_ && !this->closed_)
      {
        transaction* current = this->current_send_transaction();

        if (current)
        {
          this->send_loop_running_ = true;
          this->activity_deadline_timer_.expires_from_now(this->activity_timeout_);

          if (!current->head_sent)
          {
            if (current->outgoing_head_data.size())
            {
              current->head_sent = true;

              auto self = casted_shared_from_this();
              this->socket_->send(current->outgoing_head_data.data(), current->outgoing_head_data.size(), [self](const std::error_code &ec, std::size_t bytes_sent)
              {
                if (ec)
                {
                  self->close(ec);
                }
                else if (!self->is_closed())
                {
                  self->send_loop_running_ = false;
                  self->run_send_loop();
                }
              });
            }
            else
            {
              this->send_loop_running_ = false;
            }
          }
          else if (current->outgoing_body.size())
          {
            auto self = casted_shared_from_this();
            this->socket_->send(current->outgoing_body.front().data(), current->outgoing_body.front().size(), [self, current](const std::error_code &ec, std::size_t bytes_sent)
            {
              if (ec)
              {
                self->close(ec);
              }
              else if (!self->is_closed())
              {
                assert(current->outgoing_body.front().size() == bytes_sent);
                current->outgoing_body.pop();
                if (current->outgoing_body.empty()) // TODO: possibly check if ended.
                  current->on_drain ? current->on_drain() : void();

                self->send_loop_running_ = false;
                self->run_send_loop();
              }
            });
          }
          else if (current->outgoing_ended)
          {
            if (current->outgoing_trailer_data.size())
            {
              auto self = casted_shared_from_this();
              this->socket_->send(current->outgoing_trailer_data.data(), current->outgoing_trailer_data.size(), [self, current](const std::error_code &ec, std::size_t bytes_sent)
              {
                if (ec)
                {
                  self->close(ec);
                }
                else if (!self->is_closed())
                {
                  current->state = (current->state == transaction_state::half_closed_remote ? transaction_state::closed : transaction_state::half_closed_local);
                  if (current->state == transaction_state::closed)
                  {
                    current->on_close ? current->on_close(std::error_code()) : void();
                    self->garbage_collect_transactions();
                  }

                  self->send_loop_running_ = false;
                  self->run_send_loop();
                }
              });
            }
            else
            {
              current->state = (current->state == transaction_state::half_closed_remote ? transaction_state::closed : transaction_state::half_closed_local);
              if (current->state == transaction_state::closed)
              {
                current->on_close ? current->on_close(std::error_code()) : void();
                auto self = casted_shared_from_this();
                this->socket_->io_service().post([self]()
                {
                  self->garbage_collect_transactions();
                });
              }

              this->send_loop_running_ = false;
              this->run_send_loop();
            }
          }
          else
          {
            this->send_loop_running_ = false;
          }
        }
      }
    }

    template <typename SendMsg, typename RecvMsg>
    std::uint32_t v1_connection<SendMsg, RecvMsg>::create_stream(std::uint32_t dependency_transaction_id, std::uint32_t transaction_id)
    {
      this->transaction_queue_.emplace_back(transaction(this->next_transaction_id_));
      return this->next_transaction_id_++;
    }

    template <typename SendMsg, typename RecvMsg>
    bool v1_connection<SendMsg, RecvMsg>::send_message_head(std::uint64_t transaction_id, const v1_message_head& head)
    {
      bool ret = false;

      auto it = std::find_if(this->transaction_queue_.begin(), this->transaction_queue_.end(), [transaction_id](const transaction& m) { return m.id == transaction_id; });
      if (it != this->transaction_queue_.end() && !it->head_sent)
      {

        std::stringstream ss;
        v1_message_head::serialize(head, ss);
        it->outgoing_head_data = std::move(ss.str());

        this->run_send_loop();
        ret = true;
      }

      return ret;
    }

    template <>
    bool v1_connection<request_head, response_head>::send_headers(std::uint32_t transaction_id, const request_head& head, bool end_headers, bool end_stream)
    {
      bool ret = false;

      v1_request_head v1_head(head);

      if (!v1_head.header_exists("user-agent"))
        v1_head.header("user-agent", "Manifold"); // TODO: Allow overide of default for connection.

      std::string method = v1_head.method();
      std::transform(method.begin(), method.end(), method.begin(), ::toupper);
      if (method != "GET" && method != "HEAD")
        v1_head.header("transfer-encoding", "chunked");
      else
      {
        v1_head.remove_header("transfer-encoding");
        v1_head.remove_header("content-length");

        auto it = std::find_if(this->transaction_queue_.begin(), this->transaction_queue_.end(), [transaction_id](const transaction &t) { return t.id == transaction_id; });
        if (it != this->transaction_queue_.end())
        {
          it->outgoing_ended = true;
          if (method == "HEAD")
            it->ignore_incoming_body = true;
        }
      }

      if (this->send_message_head(transaction_id, v1_head))
      {
        if (end_stream)
          ret = this->end_message(transaction_id);
        else
          ret = true;
      }

      return ret;
    }

    template <>
    bool v1_connection<response_head, request_head>::send_headers(std::uint32_t transaction_id, const response_head& head, bool end_headers, bool end_stream)
    {
      bool ret = false;

      v1_response_head v1_head(head);

      if (!v1_head.header_exists("server"))
        v1_head.header("server", "Manifold"); // TODO: Allow overide of default for connection.

      v1_head.header("transfer-encoding", "chunked");

      if (this->send_message_head(transaction_id, v1_head))
      {
        if (end_stream)
          ret = this->end_message(transaction_id);
        else
          ret = true;
      }

      return ret;
    }
#ifndef MANIFOLD_REMOVED_TRAILERS
    template <typename SendMsg, typename RecvMsg>
    bool v1_connection<SendMsg, RecvMsg>::send_trailers(std::uint32_t stream_id, const header_block& head, bool end_headers, bool end_stream)
    {
      return this->end_message(stream_id, v1_header_block(head));
    }
#endif

    template <typename SendMsg, typename RecvMsg>
    bool v1_connection<SendMsg, RecvMsg>::send_data(std::uint32_t transaction_id, const char*const data, std::uint32_t data_sz, bool end_stream)
    {
      bool ret = false;

      auto it = std::find_if(this->transaction_queue_.begin(), this->transaction_queue_.end(), [transaction_id](const transaction& t) { return t.id == transaction_id; });
      if (it != this->transaction_queue_.end() && it->outgoing_head_data.size() && !it->outgoing_ended)
      {
        if (data_sz)
        {
          std::stringstream ss;
          ss << std::hex << data_sz;
          std::string size_line(ss.str() + "\r\n");
          std::vector<char> tmp(size_line.size() + data_sz + 2);
          std::memcpy(tmp.data(), size_line.data(), size_line.size());
          std::memcpy(tmp.data() + size_line.size(), data, data_sz);
          std::memcpy(tmp.data() + size_line.size() + data_sz, "\r\n", 2);
          it->outgoing_body.push(std::move(tmp));
        }

        if (end_stream)
          ret = this->end_message(transaction_id);
        else
        {
          this->run_send_loop();
          ret = true;
        }
      }

      return ret;
    }

    template <typename SendMsg, typename RecvMsg>
    bool v1_connection<SendMsg, RecvMsg>::end_message(std::uint32_t transaction_id, const v1_header_block& trailers)
    {
      bool ret = false;

      auto it = std::find_if(this->transaction_queue_.begin(), this->transaction_queue_.end(), [transaction_id](const transaction& t) { return t.id == transaction_id; });
      if (it != this->transaction_queue_.end() && it->outgoing_head_data.size() && !it->outgoing_ended)
      {

        std::string size_line("0\r\n");
        std::stringstream ss;
        v1_header_block::serialize(trailers, ss);
        size_line.append(ss.str());
        it->outgoing_trailer_data = std::move(size_line);

        it->outgoing_ended = true;

        this->run_send_loop();
        ret = true;
      }

      return ret;
    }

    template class v1_connection<request_head, response_head>;
    template class v1_connection<response_head, request_head>;
  }
}

