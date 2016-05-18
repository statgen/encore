#pragma once
#ifndef MANIFOLD_HTTP_V1_CONNECTION_HPP
#define MANIFOLD_HTTP_V1_CONNECTION_HPP

#include "socket.hpp"
#include "http_connection.hpp"
#include "http_v1_message_head.hpp"

#include <functional>
#include <deque>
#include <queue>
#include <memory>
#include <chrono>

namespace manifold
{
  namespace http
  {
    template <typename SendMsg, typename RecvMsg>
    class v1_connection : public connection<SendMsg, RecvMsg>
    {
    private:
      v1_connection(socket* new_sock)
        : socket_(new_sock),
          closed_(false),
        //recv_buffer_(8192),
          next_transaction_id_(1),
          send_loop_running_(false),
          activity_deadline_timer_(socket_->io_service())

      {
      }
    public:
      v1_connection(non_tls_socket&& sock)
        : v1_connection(new non_tls_socket(std::move(sock)))
      {
      }

      v1_connection(tls_socket&& sock)
        : v1_connection(new tls_socket(std::move(sock)))
      {
      }

      virtual ~v1_connection()
      {
        std::cout << "~v1_connection()" << std::endl;
        this->socket_->close();
      }

      ::manifold::http::version version() const { return version::http1_1; }
      void run(std::chrono::system_clock::duration timeout);
      void close(const std::error_code& ec);
      bool is_closed() const;

      void on_close(const std::function<void(const std::error_code& ec)>& fn);
      void on_new_stream(const std::function<void(std::uint32_t transaction_id)>& fn);

      void on_headers(std::uint32_t transaction_id, const std::function<void(RecvMsg&& headers)>& fn);
      void on_informational_headers(std::uint32_t transaction_id, const std::function<void(RecvMsg&& headers)>& fn);
#ifdef MANIFOLD_REMOVED_TRAILERS
      void on_trailers(std::uint32_t transaction_id, const std::function<void(header_block&& headers)>& fn);
#endif
      void on_push_promise(std::uint32_t transaction_id, const std::function<void(SendMsg&& headers, std::uint32_t promised_transaction_id)>& fn) {}
      void on_data(std::uint32_t transaction_id, const std::function<void(const char* const buf, std::size_t buf_size)>& fn);
      void on_close(std::uint32_t transaction_id, const std::function<void(const std::error_code& ec)>& fn);
      void on_end(std::uint32_t transaction_id, const std::function<void()>& fn);
      void on_drain(std::uint32_t transaction_id, const std::function<void()>& fn);


      std::uint32_t create_stream(std::uint32_t dependency_transaction_id, std::uint32_t transaction_id);
      bool send_data(std::uint32_t stream_id, const char *const data, std::uint32_t data_sz, bool end_stream);
      bool send_headers(std::uint32_t stream_id, const SendMsg& head, bool end_headers, bool end_stream);
#ifndef MANIFOLD_REMOVED_TRAILERS
      bool send_trailers(std::uint32_t stream_id, const header_block& head, bool end_headers, bool end_stream);
#endif
      //bool send_headers(std::uint32_t stream_id, const v2_header_block& head, priority_options priority, bool end_headers, bool end_stream);
      //bool send_priority(std::uint32_t stream_id, priority_options options);
      void send_reset_stream(std::uint32_t stream_id, http::v2_errc error_code)  { this->close(error_code); }
      std::uint32_t send_push_promise(std::uint32_t stream_id, const RecvMsg& head) { return 0; }
      void send_goaway(http::v2_errc error_code, const char *const data = nullptr, std::uint32_t data_sz = 0) { this->close(error_code); }

      bool send_message_head(std::uint64_t transaction_id, const v1_message_head& head);
      //bool send_message_body(std::uint64_t transaction_id, const char*const data, std::size_t data_sz);
      bool end_message(std::uint32_t transaction_id, const v1_header_block& trailers = v1_header_block());
    private:
      std::shared_ptr<v1_connection> casted_shared_from_this()
      {
        return std::dynamic_pointer_cast<v1_connection<SendMsg, RecvMsg>>(connection<SendMsg, RecvMsg>::shared_from_this());
      }

      enum class transaction_state
      {
        open = 1,
        half_closed_local,
        half_closed_remote,
        closed
      };

      struct transaction
      {
        std::string outgoing_head_data;
        bool head_sent = false;
        std::queue<std::vector<char>> outgoing_body;
        std::string outgoing_trailer_data;
        bool outgoing_ended = false;

        std::function<void(RecvMsg&&)> on_headers;
        std::function<void(RecvMsg&&)> on_informational_headers;
        std::function<void(header_block&&)> on_trailers;
        bool ignore_incoming_body = false;
        v1_header_block incoming_trailers;
        std::function<void(const char* const, std::size_t)> on_data;
        std::function<void()> on_end;
        std::function<void()> on_drain;

        const std::uint32_t id;
        transaction_state state = transaction_state::open;
        std::function<void(const std::error_code& ec)> on_close;
        transaction(std::uint32_t transaction_id)
          : id(transaction_id)
        {
        }
      };

      std::unique_ptr<socket> socket_;
      bool closed_;
      std::array<char, 8192> recv_buffer_;
      //asio::streambuf recv_buffer_;
      std::deque<transaction> transaction_queue_;
      std::uint32_t next_transaction_id_;
      bool send_loop_running_;
      std::chrono::system_clock::duration activity_timeout_;
      asio::basic_waitable_timer<std::chrono::system_clock> activity_deadline_timer_;


      std::function<void(const std::error_code&)> on_close_;
      std::function<void(std::uint32_t)> on_new_stream_;

      static bool incoming_head_is_head_request(const RecvMsg& head);
      static bool deserialize_incoming_headers(std::istream& is, RecvMsg& generic_headers);
      void garbage_collect_transactions();
      transaction* current_send_transaction();
      transaction* current_recv_transaction();

      void run_timeout_loop(const std::error_code& ec = std::error_code());
      void run_send_loop();
      void run_recv_loop();
      void recv_headers();
      void recv_chunk_encoded_body();
      void recv_chunk_data(std::size_t chunk_size);
      void recv_trailers();
      void recv_known_length_body(std::uint64_t content_length);
    };
  }
}

#endif //MANIFOLD_HTTP_V1_CONNECTION_HPP
