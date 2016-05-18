#pragma once

#ifndef MANIFOLD_HTTP_V2_CONNECTION_HPP
#define MANIFOLD_HTTP_V2_CONNECTION_HPP

#include <random>
#include <list>
#include <queue>
#include <map>
#include <memory>
#include <vector>
#include <queue>

#include "socket.hpp"
#include "http_frame.hpp"
#include "http_v2_message_head.hpp"
#include "http_response_head.hpp"
#include "http_request_head.hpp"
#include "http_connection.hpp"

#ifndef MANIFOLD_DISABLE_HTTP2

namespace manifold
{
  namespace http
  {

//    //================================================================//
//    class connection_io_impl
//    {
//    public:
//      connection_io_impl() {}
//      virtual ~connection_io_impl() {}
//      virtual void recv_frame(frame& destination, const std::function<void(const std::error_code& ec)>& cb) = 0;
//      virtual void send_frame(const frame& source, const std::function<void(const std::error_code& ec)>& cb) = 0;
//      virtual bool is_encrypted() const = 0;
//      virtual asio::ip::tcp::socket::lowest_layer_type& socket() = 0;
//    };
//    //================================================================//

//    //================================================================//
//    class tls_connection_io_impl : public connection_io_impl
//    {
//    private:
//      manifold::tls_socket socket_stream_;
//    protected:
//      void recv_frame(frame& destination, const std::function<void(const std::error_code& ec)>& cb)
//      {
//        frame::recv_frame(this->socket_stream_, destination, cb);
//      }
//      void send_frame(const frame& source, const std::function<void(const std::error_code& ec)>& cb)
//      {
//        frame::send_frame(this->socket_stream_, source, cb);
//      }
//    public:
//      tls_connection_io_impl(manifold::tls_socket&& sock)
//          : socket_stream_(std::move(sock))
//      {}
//      ~tls_connection_io_impl() {}
//
//      asio::ip::tcp::socket::lowest_layer_type& socket() { return this->ssl_stream().lowest_layer(); }
//      asio::ssl::stream<asio::ip::tcp::socket>& ssl_stream() { return this->socket_stream_; }
//      bool is_encrypted() const { return true; }
//    };
//    //================================================================//
//
//    //================================================================//
//    class non_tls_connection_io_impl : public connection_io_impl
//    {
//    private:
//      manifold::socket socket_;
//    protected:
//      void recv_frame(frame& destination, const std::function<void(const std::error_code& ec)>& cb)
//      {
//        frame::recv_frame(this->socket_, destination, cb);
//      }
//      void send_frame(const frame& source, const std::function<void(const std::error_code& ec)>& cb)
//      {
//        frame::send_frame(this->socket_, source, cb);
//      }
//    public:
//      non_tls_connection_io_impl(asio::io_service& ioservice)
//        : socket_(ioservice)
//      {}
//      ~non_tls_connection_io_impl() {}
//
//      asio::ip::tcp::socket::lowest_layer_type& socket() { return this->socket_; }
//      asio::ip::tcp::socket& raw_socket() { return this->socket_; }
//      bool is_encrypted() const { return false; }
//    };
//    //================================================================//

    //================================================================//
    template <typename SendMsg, typename RecvMsg>
    class v2_connection : public connection<SendMsg, RecvMsg>
    {
    public:
      //================================================================//
      enum class setting_code
      {
        header_table_size      = 0x1, // 4096
        enable_push            = 0x2, // 1
        max_concurrent_streams = 0x3, // (infinite)
        initial_window_size    = 0x4, // 65535
        max_frame_size         = 0x5, // 16384
        max_header_list_size   = 0x6  // (infinite)
      };
      //================================================================//
    protected:
      static const std::uint32_t default_header_table_size      ; //= 4096;
      static const std::uint32_t default_enable_push            ; //= 1;
      static const std::uint32_t default_initial_window_size    ; //= 65535;
      static const std::uint32_t default_max_frame_size         ; //= 16384;

      //================================================================//
      enum class stream_state
      {
        idle = 0,
        reserved_local,
        reserved_remote,
        open,
        half_closed_local,
        half_closed_remote,
        closed
      };

      class stream
      {
      protected:
        const std::uint32_t id_;
        std::queue<frame>& outgoing_non_data_frames_;
        stream_state state_ = stream_state::idle;
        bool on_headers_called_ = false;
        std::function<void(const char* const buf, std::size_t buf_size)> on_data_;
        std::function<void(RecvMsg&& headers)> on_headers_;
        std::function<void(RecvMsg&& headers)> on_informational_headers_;
        std::function<void(header_block&& headers)> on_trailers_;
        std::function<void(std::uint32_t error_code)> on_rst_stream_;
        std::function<void(SendMsg&& headers, std::uint32_t promised_stream_id)> on_push_promise_;
        std::function<void()> on_end_;
        std::function<void()> on_drain_;
        std::function<void(const std::error_code& error_code)> on_close_;

        std::queue<data_frame> outgoing_data_frames_;
        std::int32_t incoming_window_size_ = 65535;
        std::int32_t outgoing_window_size_ = 65535;
        std::uint32_t stream_dependency_id_ = 0;
        std::uint8_t weight_ = 16;
        bool end_stream_frame_received_ = false;

        static bool incoming_header_is_informational(const RecvMsg &head);
      public:
        //const std::function<void(const char* const buf, std::size_t buf_size)>& on_data() const { return this->on_data_; }
        //const std::function<void(http::header_block&& headers)>& on_headers() const { return this->on_headers_; }
        //const std::function<void(std::uint32_t error_code)>& on_rst_stream() const { return this->on_rst_stream_; }
        //const std::function<void(http::header_block&& headers, std::uint32_t promised_stream_id)>& on_push_promise() const { return this->on_push_promise_; }
        //const std::function<void()>& on_end() const { return this->on_end_; }
        //const std::function<void()>& on_drain() const { return this->on_drain_; }
        //const std::function<void(std::uint32_t error_code)>& on_close() const { return this->on_close_; }

        void on_data(const std::function<void(const char* const buf, std::size_t buf_size)>& fn);
        void on_headers(const std::function<void(RecvMsg&& headers)>& fn);
        void on_informational_headers(const std::function<void(RecvMsg&& headers)>& fn);
        void on_trailers(const std::function<void(header_block&& headers)>& fn);
        void on_rst_stream(const std::function<void(std::uint32_t error_code)>& fn);
        void on_push_promise(const std::function<void(SendMsg&& headers, std::uint32_t promised_stream_id)>& fn);
        void on_end(const std::function<void()>& fn);
        void on_drain(const std::function<void()>& fn);
        void on_close(const std::function<void(const std::error_code& ec)>& fn);

        stream_state state() const { return this->state_; }
        std::uint32_t id() const { return this->id_; }
        bool has_data_frame();
        bool has_sendable_data_frame();
        frame pop_data_frame(std::uint32_t connection_window_size);
        bool adjust_local_window_size(std::int32_t amount);
        bool adjust_peer_window_size(std::int32_t amount);

        //std::queue<frame> outgoing_non_data_frames;
        stream(std::uint32_t stream_id, std::queue<frame>& connection_outgoing_queue, uint32_t initial_window_size, uint32_t initial_peer_window_size)
          : id_(stream_id), outgoing_non_data_frames_(connection_outgoing_queue), incoming_window_size_(initial_window_size), outgoing_window_size_(initial_peer_window_size) {}
        stream(stream&& source)
          : id_(std::move(source.id_)),
            outgoing_non_data_frames_(source.outgoing_non_data_frames_),
            state_(std::move(source.state_)),
            on_headers_called_(std::move(source.on_headers_called_)),
            on_data_(std::move(source.on_data_)),
            on_headers_(std::move(source.on_headers_)),
            on_informational_headers_(std::move(source.on_informational_headers_)),
            on_trailers_(std::move(source.on_trailers_)),
            on_rst_stream_(std::move(source.on_rst_stream_)),
            on_push_promise_(std::move(source.on_push_promise_)),
            on_end_(std::move(source.on_end_)),
            on_drain_(std::move(source.on_drain_)),
            on_close_(std::move(source.on_close_)),
            //incoming_data_frames(std::move(source.incoming_data_frames)),
            //outgoing_non_data_frames(std::move(source.outgoing_non_data_frames)),
            outgoing_data_frames_(std::move(source.outgoing_data_frames_)),
            incoming_window_size_(std::move(source.incoming_window_size_)),
            outgoing_window_size_(std::move(source.outgoing_window_size_)),
            stream_dependency_id_(std::move(source.stream_dependency_id_)),
            weight_(std::move(source.weight_)),
            end_stream_frame_received_(std::move(source.end_stream_frame_received_))
        {
        }
        ~stream() {}

        //----------------------------------------------------------------//
        void handle_incoming_frame(const data_frame& incoming_data_frame, std::int32_t local_initial_window_size, v2_errc& connection_error);
        void handle_incoming_frame(const headers_frame& incoming_headers_frame, const std::vector<continuation_frame>& continuation_frames, hpack::decoder& dec, v2_errc& connection_error);
        void handle_incoming_frame(const priority_frame& incoming_priority_frame, v2_errc& connection_error);
        void handle_incoming_frame(const rst_stream_frame& incoming_rst_stream_frame, v2_errc& connection_error);
        void handle_incoming_frame(const push_promise_frame& incoming_push_promise_frame, const std::vector<continuation_frame>& continuation_frames, hpack::decoder& dec, stream& idle_promised_stream, v2_errc& connection_error);
        void handle_incoming_frame(const window_update_frame& incoming_window_update_frame, v2_errc& connection_error);
        //----------------------------------------------------------------//

        //----------------------------------------------------------------//
        bool send_data_frame(const char*const data, std::uint32_t data_sz, bool end_stream, std::uint32_t max_frame_size);
        bool send_headers_frame(const v2_header_block& headers, bool end_stream, hpack::encoder& enc, std::uint32_t max_frame_size);
        bool handle_outgoing_priority_frame_state_change();
        bool send_rst_stream_frame(v2_errc ec);
        bool send_push_promise_frame(const v2_header_block& headers, stream& promised_stream, hpack::encoder& enc, std::uint32_t max_frame_size);
        bool send_window_update_frame(std::int32_t amount);

//        bool handle_outgoing_headers_state_change();
//        bool handle_outgoing_end_stream_state_change();
//        bool handle_outgoing_push_promise_state_change();
//        bool handle_outgoing_rst_stream_state_change(errc ec);
        //----------------------------------------------------------------//
      };
      //================================================================//
#ifndef MANIFOLD_REMOVED_PRIORITY
      //================================================================//
      class stream_dependency_tree_child_node;
      class stream_dependency_tree
      {
      protected:
        //----------------------------------------------------------------//
        std::vector<stream_dependency_tree_child_node> children_;
        //----------------------------------------------------------------//
      public:
        //----------------------------------------------------------------//
        stream_dependency_tree();
        stream_dependency_tree(const std::vector<stream_dependency_tree_child_node>& children);
        virtual ~stream_dependency_tree() {}
        //----------------------------------------------------------------//


        //----------------------------------------------------------------//
        const std::vector<stream_dependency_tree_child_node>& children() const;
        void insert_child(stream_dependency_tree_child_node&& child);
        void remove(stream& stream_to_remove);
        void clear_children();

        stream* get_next_send_stream_ptr(std::uint32_t connection_window_size, std::minstd_rand& rng);
        //----------------------------------------------------------------//

        //----------------------------------------------------------------//
        //----------------------------------------------------------------//
      };
      //================================================================//

      //================================================================//
      class stream_dependency_tree_child_node : public stream_dependency_tree
      {
      private:
        //----------------------------------------------------------------//
        stream* stream_ptr_;
        //----------------------------------------------------------------//
      public:
        //----------------------------------------------------------------//
        stream_dependency_tree_child_node(stream* stream_ptr)
          : stream_ptr_(stream_ptr) {}
        stream_dependency_tree_child_node(stream* stream_ptr, const std::vector<stream_dependency_tree_child_node>& children)
          : stream_dependency_tree(children), stream_ptr_(stream_ptr) {}
        ~stream_dependency_tree_child_node() {}
        //----------------------------------------------------------------//


        //----------------------------------------------------------------//
        stream* stream_ptr() const;
        bool check_for_outgoing_frame(bool can_send_data);
        stream* get_next_send_stream_ptr(std::uint32_t connection_window_size, std::minstd_rand& rng);
        //----------------------------------------------------------------//
      };
      //================================================================//
#endif
    private:
      //----------------------------------------------------------------//
      std::unique_ptr<socket> socket_;
      std::unordered_map<std::uint32_t,stream> streams_;
      std::map<setting_code,std::uint32_t> peer_settings_;
      std::map<setting_code,std::uint32_t> local_settings_;
      std::queue<std::list<std::pair<std::uint16_t,std::uint32_t>>> pending_local_settings_;
      hpack::encoder hpack_encoder_;
      hpack::decoder hpack_decoder_;
      std::minstd_rand rng_;
      bool started_;
      bool closed_;
      bool send_loop_running_;
      std::chrono::system_clock::duration data_transfer_timeout_;
      asio::basic_waitable_timer<std::chrono::system_clock> data_transfer_deadline_timer_;
      std::uint32_t last_newly_accepted_stream_id_;
      std::uint32_t next_stream_id_;
      std::int32_t outgoing_window_size_;
      std::int32_t incoming_window_size_;
      http::frame incoming_frame_;
      std::queue<http::frame> incoming_header_block_fragments_;
      http::frame outgoing_frame_;
      std::queue<frame> outgoing_frames_;
#ifndef MANIFOLD_REMOVED_PRIORITY
      stream_dependency_tree stream_dependency_tree_;
#endif
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      // Connection level callbacks:
      std::function<void(std::uint32_t stream_id)> on_new_stream_;
      std::function<void(const std::error_code& error_code)> on_close_;
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      std::uint32_t get_next_stream_id();
      std::int32_t connection_level_outgoing_window_size() const { return this->outgoing_window_size_; }
      std::int32_t connection_level_incoming_window_size() const { return this->incoming_window_size_; }
      void garbage_collect_streams();
      bool receiving_push_promise_is_allowed();
      bool sending_push_promise_is_allowed();
      typename std::unordered_map<std::uint32_t, stream>::iterator find_stream_with_data();
      static bool i_am_server();
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      void send_connection_level_window_update(std::uint32_t amount);
      void send_ping_acknowledgement(std::uint64_t opaque_data);
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      void run_timeout_loop(const std::error_code& ec = std::error_code());
      void run_recv_loop();
      void run_send_loop();
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      void handle_incoming_frame(const settings_frame& incoming_settings_frame);
      void handle_incoming_frame(const ping_frame& incoming_ping_frame);
      void handle_incoming_frame(const goaway_frame& incoming_goaway_frame);
      void handle_incoming_frame(const window_update_frame& incoming_window_update_frame);
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      std::shared_ptr<v2_connection> casted_shared_from_this()
      {
        return std::dynamic_pointer_cast<v2_connection<SendMsg, RecvMsg>>(connection<SendMsg, RecvMsg>::shared_from_this());
      }
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      v2_connection(socket*);
      //----------------------------------------------------------------//
    public:
      //----------------------------------------------------------------//
      v2_connection(non_tls_socket&& sock);
      v2_connection(tls_socket&& sock);
      virtual ~v2_connection();
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      const std::map<setting_code,std::uint32_t>& local_settings() const { return this->local_settings_; }
      const std::map<setting_code,std::uint32_t>& peer_settings() const { return this->peer_settings_; }
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      static const std::array<char,24> preface;
      static const std::uint32_t max_stream_id = 0x7FFFFFFF;
      static const std::uint32_t initial_stream_id;
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      ::manifold::http::version version() const { return version::http2; }
      void run(std::chrono::system_clock::duration timeout, const std::list<std::pair<setting_code, std::uint32_t>>& custom_settings);
      void close(const std::error_code& ec);
      bool is_closed() const;
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      void on_new_stream(const std::function<void(std::uint32_t stream_id)>& fn);
      void on_close(const std::function<void(const std::error_code& ec)>& fn);
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      // connection-only frames: settings, ping, goaway
      // window_update is for both.
      void on_headers(std::uint32_t stream_id, const std::function<void(RecvMsg&& headers)>& fn);
      void on_informational_headers(std::uint32_t stream_id, const std::function<void(RecvMsg&& headers)>& fn);
#ifndef MANIFOLD_REMOVED_TRAILERS
      void on_trailers(std::uint32_t stream_id, const std::function<void(header_block&& headers)>& fn);
#endif
      void on_data(std::uint32_t stream_id, const std::function<void(const char* const buf, std::size_t buf_size)>& fn);
      //void on_headers(std::uint32_t stream_id, const std::function<void(v2_header_block&& headers)>& fn);
      void on_close(std::uint32_t stream_id, const std::function<void(const std::error_code& ec)>& fn);
      void on_push_promise(std::uint32_t stream_id, const std::function<void(SendMsg&& headers, std::uint32_t promised_stream_id)>& fn);


      void on_end(std::uint32_t stream_id, const std::function<void()>& fn);
      //void on_window_update(std::uint32_t stream_id, const std::function<void()>& fn);
      void on_drain(std::uint32_t stream_id, const std::function<void()>& fn);
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      std::uint32_t create_stream(std::uint32_t dependency_stream_id, std::uint32_t stream_id);
      bool send_data(std::uint32_t stream_id, const char *const data, std::uint32_t data_sz, bool end_stream);
      bool send_headers(std::uint32_t stream_id, const SendMsg& head, bool end_headers, bool end_stream);
#ifndef MANIFOLD_REMOVED_TRAILERS
      bool send_trailers(std::uint32_t stream_id, const header_block& head, bool end_headers, bool end_stream);
#endif
      bool send_headers(std::uint32_t stream_id, v2_header_block&& head, bool end_headers, bool end_stream);
      bool send_headers(std::uint32_t stream_id, v2_header_block&& head, priority_options priority, bool end_headers, bool end_stream);
      bool send_priority(std::uint32_t stream_id, priority_options options);
      void send_reset_stream(std::uint32_t stream_id, http::v2_errc error_code);
      void send_settings(const std::list<std::pair<std::uint16_t,std::uint32_t>>& settings);
      std::uint32_t send_push_promise(std::uint32_t stream_id, const RecvMsg& head);
      std::uint32_t send_push_promise(std::uint32_t stream_id, v2_header_block&& head);
      void send_ping(std::uint64_t opaque_data);
      void send_goaway(http::v2_errc error_code, const char *const data = nullptr, std::uint32_t data_sz = 0);
      bool send_window_update(std::uint32_t stream_id, std::uint32_t amount);
      //bool send_countinuation(std::uint32_t stream_id, const v2_header_block& head, bool end_headers);
      //----------------------------------------------------------------//


      //void send(char* buf, std::size_t buf_size, const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& handler);
      //void recv(const char* buf, std::size_t buf_size, const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& handler);
    };
    //================================================================//
  }
}

#endif //MANIFOLD_DISABLE_HTTP2

#endif //MANIFOLD_HTTP_V2_CONNECTION_HPP