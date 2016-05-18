
#include <iostream>
#include <http_v2_request_head.hpp>
#include <http_v2_response_head.hpp>

#include "http_v2_connection.hpp"

#ifndef MANIFOLD_DISABLE_HTTP2

namespace manifold
{
  namespace http
  {
    template <typename SendMsg, typename RecvMsg> const std::uint32_t v2_connection<SendMsg, RecvMsg>::default_header_table_size      = 4096;
    template <typename SendMsg, typename RecvMsg> const std::uint32_t v2_connection<SendMsg, RecvMsg>::default_enable_push            = 1;
    template <typename SendMsg, typename RecvMsg> const std::uint32_t v2_connection<SendMsg, RecvMsg>::default_initial_window_size    = 65535;
    template <typename SendMsg, typename RecvMsg> const std::uint32_t v2_connection<SendMsg, RecvMsg>::default_max_frame_size         = 16384;
#ifndef MANIFOLD_REMOVED_PRIORITY
    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::stream_dependency_tree()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::stream_dependency_tree(const std::vector<stream_dependency_tree_child_node>& children)
      : children_(children)
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    typename v2_connection<SendMsg, RecvMsg>::stream* v2_connection<SendMsg, RecvMsg>::stream_dependency_tree_child_node::stream_ptr() const
    {
      return this->stream_ptr_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    const std::vector<typename v2_connection<SendMsg, RecvMsg>::stream_dependency_tree_child_node>& v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::children() const
    {
      return this->children_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::insert_child(v2_connection<SendMsg, RecvMsg>::stream_dependency_tree_child_node&& child)
    {
      this->children_.push_back(std::move(child));
    }
    //----------------------------------------------------------------//

    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::remove(stream& stream_to_remove)
    {
      bool found = false;
      for (auto it = this->children_.begin(); it != this->children_.end() && found == false; ++it)
      {
        if (it->stream_ptr() == &stream_to_remove)
        {
          stream_dependency_tree tree_to_remove(std::move(*it));
          this->children_.erase(it);

          this->children_.reserve(this->children_.size() + tree_to_remove.children_.size());
          for (auto child_it = tree_to_remove.children_.begin(); child_it != tree_to_remove.children_.end(); ++child_it)
          {
            this->children_.push_back(std::move(*child_it));
          }
          tree_to_remove.children_.clear();
          found = true;
        }
      }

      for (auto it = this->children_.begin(); it != this->children_.end() && found == false; ++it)
        it->remove(stream_to_remove);
    }

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::clear_children()
    {
      for (auto it = this->children_.begin(); it != this->children_.end(); ++it)
        it->clear_children();
      this->children_.clear();
    }
    //----------------------------------------------------------------//
#endif
    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    const std::array<char,24> v2_connection<SendMsg, RecvMsg>::preface{{0x50, 0x52, 0x49, 0x20, 0x2a, 0x20, 0x48, 0x54, 0x54, 0x50, 0x2f, 0x32, 0x2e, 0x30, 0x0d, 0x0a, 0x0d, 0x0a, 0x53, 0x4d, 0x0d, 0x0a, 0x0d, 0x0a}};
    template<> const std::uint32_t v2_connection<request_head, response_head>::initial_stream_id = 1;
    template<> const std::uint32_t v2_connection<response_head, request_head>::initial_stream_id = 2;
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    v2_connection<SendMsg, RecvMsg>::v2_connection(socket* new_sock)
      : socket_(new_sock),
        hpack_encoder_(default_header_table_size),
        hpack_decoder_(default_header_table_size),
        data_transfer_deadline_timer_(socket_->io_service()),
        last_newly_accepted_stream_id_(0),
        next_stream_id_(initial_stream_id),
        outgoing_window_size_(default_initial_window_size),
        incoming_window_size_(default_initial_window_size)
    {
      std::seed_seq seed({static_cast<std::uint32_t>((std::uint64_t)this)}); // Entropy is not important here, since rng is for priority distribution.
      this->rng_.seed(seed);

      // header_table_size      = 0x1, // 4096
      // enable_push            = 0x2, // 1
      // max_concurrent_streams = 0x3, // (infinite)
      // initial_window_size    = 0x4, // 65535
      // max_frame_size         = 0x5, // 16384
      // max_header_list_size   = 0x6  // (infinite)
      this->local_settings_ =
        {
          { setting_code::header_table_size,   default_header_table_size },
          { setting_code::enable_push,         default_enable_push },
          { setting_code::initial_window_size, default_initial_window_size },
          { setting_code::max_frame_size,      default_max_frame_size }
        };

      this->peer_settings_ = this->local_settings_;

      this->started_ = false;
      this->closed_ = false;
      this->send_loop_running_ = false;

    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    v2_connection<SendMsg, RecvMsg>::v2_connection(non_tls_socket&& sock)
      : v2_connection(new non_tls_socket(std::move(sock)))
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    v2_connection<SendMsg, RecvMsg>::v2_connection(tls_socket&& sock)
      : v2_connection(new tls_socket(std::move(sock)))
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    v2_connection<SendMsg, RecvMsg>::~v2_connection()
    {
      std::cout << "~v2_connection()" << std::endl;

      this->socket_->close();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::close(const std::error_code& ec)
    {
      if (!this->closed_)
      {
        this->closed_ = true;

        //TODO: This needs to be fixed.
        this->send_goaway(v2_errc::internal_error);

        auto self = casted_shared_from_this();
        this->socket_->io_service().post([self, ec]()
        {
          for (auto it = self->streams_.begin(); it != self->streams_.end(); ++it)
          {
            if (it->second.state() != stream_state::closed)
            {
              it->second.send_rst_stream_frame(v2_errc::internal_error); // Frame will never actually be sent. This is just being called to change state and call callback.
            }
          }

          self->on_close_ ? self->on_close_(ec) : void();

          self->data_transfer_deadline_timer_.cancel();
        });
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::is_closed() const
    {
      return this->closed_;
    }
    //----------------------------------------------------------------//
#ifndef MANIFOLD_REMOVED_PRIORITY
    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    typename v2_connection<SendMsg, RecvMsg>::stream* v2_connection<SendMsg, RecvMsg>::stream_dependency_tree::get_next_send_stream_ptr(std::uint32_t connection_window_size, std::minstd_rand& rng)
    {
      stream* ret = nullptr;
      std::uint64_t weight_sum = 0;
      std::vector<stream_dependency_tree_child_node*> pool;

      for (auto it = this->children_.begin(); it != this->children_.end(); ++it)
      {
        if (it->check_for_outgoing_frame(connection_window_size > 0))
        {
          pool.push_back(&(*it));
          weight_sum += (it->stream_ptr()->weight + 1);
        }
      }

      if (pool.size())
      {
        std::uint64_t sum_index = (rng() % weight_sum) + 1;
        std::uint64_t current_sum = 0;
        for (auto it = pool.begin(); ret == nullptr && it != pool.end(); ++it)
        {
          stream_dependency_tree_child_node* current_pool_node = (*it);
          current_sum += (current_pool_node->stream_ptr()->weight + 1);
          if (sum_index <= current_sum)
          {
            ret = current_pool_node->get_next_send_stream_ptr(connection_window_size, rng);
          }
        }
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    typename v2_connection<SendMsg, RecvMsg>::stream* v2_connection<SendMsg, RecvMsg>::stream_dependency_tree_child_node::get_next_send_stream_ptr(std::uint32_t connection_window_size, std::minstd_rand& rng)
    {
      // TODO: enforce a max tree depth of 10 to avoid stack overflow from recursion.
      stream* ret = nullptr;

      if (this->stream_ptr()->has_sendable_frame(connection_window_size > 0))
      {
        ret = this->stream_ptr();
      }
      else
      {
        ret = stream_dependency_tree::get_next_send_stream_ptr(connection_window_size, rng);
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream_dependency_tree_child_node::check_for_outgoing_frame(bool can_send_data)
    {
      bool ret = false;

      if (this->stream_ptr_->has_sendable_frame(can_send_data))
        ret = true;

      for (auto it = this->children_.begin(); !ret && it != this->children_.end(); ++it)
      {
        ret = it->check_for_outgoing_frame(can_send_data);
      }

      return ret;
    }
    //----------------------------------------------------------------//
#endif

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::garbage_collect_streams()
    {
      for (auto it = this->streams_.begin(); it != this->streams_.end(); )
      {
        if (it->second.state() == stream_state::closed && !it->second.has_data_frame())
        {
#ifndef MANIFOLD_REMOVED_PRIORITY
          this->stream_dependency_tree_.remove(it->second);
#endif
          it = this->streams_.erase(it);
        }
        else
        {
          ++it;
        }
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    typename std::unordered_map<std::uint32_t, typename v2_connection<SendMsg, RecvMsg>::stream>::iterator v2_connection<SendMsg, RecvMsg>::find_stream_with_data()
    {
      if (this->streams_.empty())
        return this->streams_.end();
      //return this->streams_.begin()->second.has_sendable_data_frame() ? this->streams_.begin() : this->streams_.end(); // TEMP

      auto random_stream_it = this->streams_.begin();
      std::advance(random_stream_it, this->rng_() % this->streams_.size());

      for (auto it = random_stream_it; it != this->streams_.end(); ++it)
      {
        if (it->second.has_sendable_data_frame())
          return it;
      }


      for (auto it = this->streams_.begin(); it != random_stream_it; ++it)
      {
        if (it->second.has_sendable_data_frame())
          return it;
      }

      return this->streams_.end();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template<> bool v2_connection<request_head, response_head>::receiving_push_promise_is_allowed()
    {
      bool ret = true;
      auto it = this->local_settings_.find(setting_code::enable_push);
      if (it != this->local_settings_.end() && it->second == 0)
      {
        ret = false;
      }
      return ret;
    }
    template<> bool v2_connection<response_head, request_head>::receiving_push_promise_is_allowed() { return false; }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template<> bool v2_connection<request_head, response_head>::sending_push_promise_is_allowed() { return false; }
    template<> bool v2_connection<response_head, request_head>::sending_push_promise_is_allowed()
    {
      bool ret = true;
      auto it = this->peer_settings_.find(setting_code::enable_push);
      if (it != this->peer_settings_.end() && it->second == 0)
      {
        ret = false;
      }
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::run_timeout_loop(const std::error_code& ec)
    {
      if (!this->is_closed())
      {
        auto self = casted_shared_from_this();
        if (self->data_transfer_deadline_timer_.expires_from_now() <= std::chrono::system_clock::duration(0))
          this->close(http::errc::data_transfer_timeout); // TODO: check if idle.
        else
          this->data_transfer_deadline_timer_.async_wait(std::bind(&v2_connection::run_timeout_loop, self, std::placeholders::_1));
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::run_recv_loop()
    {
      if (!this->closed_)
      {
        this->data_transfer_deadline_timer_.expires_from_now(this->data_transfer_timeout_);

        std::shared_ptr<v2_connection> self = casted_shared_from_this();
        this->incoming_frame_ = frame(); // reset incoming frame
        frame::recv_frame(*this->socket_, this->incoming_frame_, [self](const std::error_code& ec)
        {
          if (ec)
          {
            self->close(v2_errc::internal_error);
          }
          else if (!self->is_closed())
          {
            const frame_type incoming_frame_type = self->incoming_frame_.type();
            std::uint32_t incoming_stream_id = self->incoming_frame_.stream_id();
            if (incoming_stream_id)
            {
              auto current_stream_it = self->streams_.find(incoming_stream_id);

              if (current_stream_it == self->streams_.end() && i_am_server() && (incoming_stream_id % 2) != 0)
              {
                if (incoming_stream_id > self->last_newly_accepted_stream_id_)
                {
                  self->last_newly_accepted_stream_id_ = incoming_stream_id;
                  if (!self->create_stream(0, incoming_stream_id))
                  {
                    self->close(v2_errc::internal_error);
                  }
                  else
                  {
                    current_stream_it = self->streams_.find(incoming_stream_id);
                    self->on_new_stream_ ? self->on_new_stream_(incoming_stream_id) : void();
                  }
                  assert(current_stream_it != self->streams_.end());
                }
                else
                {
                  self->close(v2_errc::protocol_error);
                }
              }

              if (current_stream_it == self->streams_.end())
              {
                // TODO: Handle Error.
                //self->close(errc::protocol_error); //Ignoring for now. Sending frame resets leaves the possibility for streams to be closed while frames are in flight.
              }
              else
              {
                if (self->incoming_header_block_fragments_.size() && (incoming_frame_type != frame_type::continuation || self->incoming_frame_.stream_id() != self->incoming_header_block_fragments_.front().stream_id()))
                {
                  // TODO: connection error PROTOCOL_ERROR
                }
                else if (incoming_frame_type == frame_type::continuation)
                {
                  if (self->incoming_header_block_fragments_.empty())
                  {
                    // TODO: connection error PROTOCOL_ERROR
                  }
                  else if (!self->incoming_frame_.continuation_frame().has_end_headers_flag())
                  {
                    self->incoming_header_block_fragments_.push(std::move(self->incoming_frame_));
                  }
                  else
                  {
                    v2_errc handle_frame_conn_error = v2_errc::no_error;
                    if (self->incoming_header_block_fragments_.front().type() == frame_type::headers)
                    {
                      headers_frame h_frame(std::move(self->incoming_header_block_fragments_.front().headers_frame()));
                      self->incoming_header_block_fragments_.pop();

                      std::vector<continuation_frame> cont_frames;
                      cont_frames.reserve(self->incoming_header_block_fragments_.size());
                      while (self->incoming_header_block_fragments_.size())
                      {
                        cont_frames.push_back(std::move(self->incoming_header_block_fragments_.front().continuation_frame()));
                        self->incoming_header_block_fragments_.pop();
                      }

                      current_stream_it->second.handle_incoming_frame(h_frame, cont_frames, self->hpack_decoder_, handle_frame_conn_error);
                    }
                    else
                    {
                      push_promise_frame pp_frame(std::move(self->incoming_header_block_fragments_.front().push_promise_frame()));
                      self->incoming_header_block_fragments_.pop();

                      std::vector<continuation_frame> cont_frames;
                      cont_frames.reserve(self->incoming_header_block_fragments_.size());
                      while (self->incoming_header_block_fragments_.size())
                      {
                        cont_frames.push_back(std::move(self->incoming_header_block_fragments_.front().continuation_frame()));
                        self->incoming_header_block_fragments_.pop();
                      }

                      if (pp_frame.promised_stream_id() <= self->last_newly_accepted_stream_id_)
                      {
                        self->close(v2_errc::protocol_error);
                      }
                      else
                      {
                        self->last_newly_accepted_stream_id_ = pp_frame.promised_stream_id();
                        if (!self->create_stream(current_stream_it->second.id(), pp_frame.promised_stream_id()))
                        {
                          self->close(v2_errc::internal_error);
                        }
                        else
                        {
                          auto promised_stream_it = self->streams_.find(pp_frame.promised_stream_id());
                          current_stream_it->second.handle_incoming_frame(pp_frame, cont_frames, self->hpack_decoder_, promised_stream_it->second, handle_frame_conn_error);
                        }
                      }
                    }
                  }
                }
                else if (incoming_frame_type == frame_type::headers || incoming_frame_type == frame_type::push_promise)
                {
                  if (!(self->receiving_push_promise_is_allowed()) && incoming_frame_type == frame_type::push_promise)
                  {
                    // TODO: Connection error.
                  }
                  else
                  {
                    bool has_end_headers_flag = (incoming_frame_type == frame_type::headers ? self->incoming_frame_.headers_frame().has_end_headers_flag() : self->incoming_frame_.push_promise_frame().has_end_headers_flag());

                    if (!has_end_headers_flag)
                      self->incoming_header_block_fragments_.push(std::move(self->incoming_frame_));
                    else if (incoming_frame_type == frame_type::headers)
                    {
                      v2_errc handle_frame_conn_error = v2_errc::no_error;
                      current_stream_it->second.handle_incoming_frame(self->incoming_frame_.headers_frame(), {}, self->hpack_decoder_, handle_frame_conn_error);
                      if (handle_frame_conn_error != v2_errc::no_error)
                        self->close(handle_frame_conn_error);
                    }
                    else
                    {
                      if (self->incoming_frame_.push_promise_frame().promised_stream_id() <= self->last_newly_accepted_stream_id_)
                      {
                        self->close(v2_errc::protocol_error);
                      }
                      else
                      {
                        self->last_newly_accepted_stream_id_ = self->incoming_frame_.push_promise_frame().promised_stream_id();
                        if (!self->create_stream(current_stream_it->second.id(), self->incoming_frame_.push_promise_frame().promised_stream_id()))
                        {
                          // TODO: Handle error.
                        }
                        else
                        {
                          v2_errc handle_frame_conn_error = v2_errc::no_error;
                          auto promised_stream_it = self->streams_.find(self->incoming_frame_.push_promise_frame().promised_stream_id());
                          current_stream_it->second.handle_incoming_frame(self->incoming_frame_.push_promise_frame(), {}, self->hpack_decoder_, promised_stream_it->second, handle_frame_conn_error);
                          if (handle_frame_conn_error != v2_errc::no_error)
                            self->close(handle_frame_conn_error);
                        }
                      }
                    }
                  }
                }
                else
                {
                  v2_errc handle_frame_conn_error = v2_errc::no_error;
                  switch (incoming_frame_type)
                  {
                    case frame_type::data:
                      self->incoming_window_size_ -= self->incoming_frame_.data_frame().data_length();
                      if (self->incoming_window_size_ < self->local_settings_[setting_code::initial_window_size] / 2)
                        self->send_connection_level_window_update(self->local_settings_[setting_code::initial_window_size] - self->incoming_window_size_);

                      current_stream_it->second.handle_incoming_frame(self->incoming_frame_.data_frame(), self->local_settings_[setting_code::initial_window_size], handle_frame_conn_error);
                      break;
                    case frame_type::priority:
                      current_stream_it->second.handle_incoming_frame(self->incoming_frame_.priority_frame(), handle_frame_conn_error);
                      break;
                    case frame_type::rst_stream:
                      current_stream_it->second.handle_incoming_frame(self->incoming_frame_.rst_stream_frame(), handle_frame_conn_error);
                      break;
                    case frame_type::window_update:
                      current_stream_it->second.handle_incoming_frame(self->incoming_frame_.window_update_frame(), handle_frame_conn_error);
                      break;
                    default:
                    {
                      // ignore unknown frame type
                    }
                  }

                  if (handle_frame_conn_error != v2_errc::no_error)
                    self->close(handle_frame_conn_error);
                }
              }
            }
            else
            {
              switch (incoming_frame_type)
              {
                case frame_type::settings:
                  self->handle_incoming_frame(self->incoming_frame_.settings_frame());
                  break;
                case frame_type::ping:
                  self->handle_incoming_frame(self->incoming_frame_.ping_frame());
                  break;
                case frame_type::goaway:
                  self->handle_incoming_frame(self->incoming_frame_.goaway_frame());
                  break;
                case frame_type::window_update:
                  self->handle_incoming_frame(self->incoming_frame_.window_update_frame());
                  break;
                default:
                {
                  // TODO: error stream-only frame missing stream id_
                }
              }
            }


            self->run_send_loop(); // One of the handle_incoming frames may have pushed an outgoing frame.
            self->run_recv_loop();
          }
        });
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template<> bool v2_connection<request_head, response_head>::stream::incoming_header_is_informational(const response_head& head) { return head.has_informational_status(); }
    template<> bool v2_connection<response_head, request_head>::stream::incoming_header_is_informational(const request_head& head) { return false; }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template<> bool v2_connection<request_head, response_head>::i_am_server() { return false; }
    template<> bool v2_connection<response_head, request_head>::i_am_server() { return true; }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_data(const std::function<void(const char* const, std::size_t)>& fn)
    {
      this->on_data_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_headers(const std::function<void(RecvMsg&&)>& fn)
    {
      this->on_headers_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_informational_headers(const std::function<void(RecvMsg&&)>& fn)
    {
      this->on_informational_headers_ = fn;
    }
    //----------------------------------------------------------------//
#ifndef MANIFOLD_REMOVED_TRAILERS
    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_trailers(const std::function<void(header_block&&)>& fn)
    {
      this->on_trailers_ = fn;
    }
    //----------------------------------------------------------------//
#endif
    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_rst_stream(const std::function<void(std::uint32_t)>& fn)
    {
      this->on_rst_stream_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_push_promise(const std::function<void(SendMsg&&, std::uint32_t)>& fn)
    {
      this->on_push_promise_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_end(const std::function<void()>& fn)
    {
      if (this->state_ == stream_state::half_closed_remote || this->state_ == stream_state::closed || this->state_ == stream_state::reserved_local)
        fn ? fn() : void();
      else
        this->on_end_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_drain(const std::function<void()>& fn)
    {
      this->on_drain_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::on_close(const std::function<void(const std::error_code&)>& fn)
    {
      if (this->state_ == stream_state::closed)
        fn ? fn(v2_errc::no_error) : void(); // TODO: pass error if there is one.
      else
        this->on_close_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::has_data_frame()
    {
      return !this->outgoing_data_frames_.empty();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::has_sendable_data_frame()
    {
      bool ret = false;

      if (this->outgoing_data_frames_.size())
      {
        if (this->outgoing_window_size_ > 0 || this->outgoing_data_frames_.front().data_length() == 0)
          ret = true;
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    frame v2_connection<SendMsg, RecvMsg>::stream::pop_data_frame(std::uint32_t connection_window_size)
    {
      frame ret;

      if (this->outgoing_data_frames_.size() && this->outgoing_window_size_ > 0 && connection_window_size > 0)
      {
        if (connection_window_size > this->outgoing_window_size_)
          connection_window_size = this->outgoing_window_size_;
        if (this->outgoing_data_frames_.front().data_length() > connection_window_size)
          ret = frame(this->outgoing_data_frames_.front().split(connection_window_size), this->id_);
        else
        {
          ret = frame(std::move(this->outgoing_data_frames_.front()), this->id_);
          this->outgoing_data_frames_.pop();
        }
        this->outgoing_window_size_ -= ret.data_frame().data_length();

        if (this->outgoing_data_frames_.empty() && this->on_drain_)
          this->on_drain_();
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::adjust_local_window_size(std::int32_t amount)
    {
      if (amount > 0 && (amount + this->incoming_window_size_) < this->incoming_window_size_)
        return false; // overflow

      this->incoming_window_size_ = (amount + this->incoming_window_size_);
      return true;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::adjust_peer_window_size(std::int32_t amount)
    {
      if (amount > 0 && (amount + this->outgoing_window_size_) < this->outgoing_window_size_)
        return false; // overflow

      this->outgoing_window_size_ = (amount + this->outgoing_window_size_);
      return true;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::handle_incoming_frame(const data_frame& incoming_data_frame, std::int32_t local_initial_window_size, v2_errc& connection_error)
    {
      switch (this->state_)
      {
        case stream_state::open:
        {
          this->incoming_window_size_ -= incoming_data_frame.data_length();
          if (this->incoming_window_size_ < local_initial_window_size / 2)
            this->send_window_update_frame(local_initial_window_size - this->incoming_window_size_);

          this->on_data_ ? this->on_data_(incoming_data_frame.data(), incoming_data_frame.data_length()) : void();

          if (incoming_data_frame.has_end_stream_flag())
          {
            this->state_ = stream_state::half_closed_remote;
            this->on_end_ ? this->on_end_() : void();
          }
          break;
        }
        case stream_state::half_closed_local:
        {
          this->incoming_window_size_ -= incoming_data_frame.data_length();
          if (this->incoming_window_size_ < local_initial_window_size / 2)
            this->send_window_update_frame(local_initial_window_size - this->incoming_window_size_);

          this->on_data_ ? this->on_data_(incoming_data_frame.data(), incoming_data_frame.data_length()) : void();

          if (incoming_data_frame.has_end_stream_flag())
          {
            this->state_= stream_state::closed;
            this->on_end_ ? this->on_end_() : void();
            this->on_close_ ? this->on_close_(v2_errc::no_error) : void();
          }
          break;
        }
        default:
        {
          // TODO: deal with frame / state_ mismatch
        }
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::handle_incoming_frame(const headers_frame& incoming_headers_frame, const std::vector<continuation_frame>& continuation_frames, hpack::decoder& dec, v2_errc& connection_error)
    {
      switch (this->state_)
      {
        case stream_state::idle:
        case stream_state::open:
        case stream_state::half_closed_local:
        case stream_state::reserved_remote:
        {


          std::string serialized_header_block;
          std::size_t serialized_header_block_sz = incoming_headers_frame.header_block_fragment_length();
          for (auto it = continuation_frames.begin(); it != continuation_frames.end(); ++it)
            serialized_header_block_sz += it->header_block_fragment_length();

          serialized_header_block.reserve(serialized_header_block_sz);

          serialized_header_block.append(incoming_headers_frame.header_block_fragment(), incoming_headers_frame.header_block_fragment_length());

          for (auto it = continuation_frames.begin(); it != continuation_frames.end(); ++it)
          {
            serialized_header_block.append(it->header_block_fragment(), it->header_block_fragment_length());
          }

          v2_header_block headers;
          if (!v2_header_block::deserialize(dec, serialized_header_block, headers))
            connection_error = v2_errc::compression_error;
          else
          {
            if (this->state_ == stream_state::reserved_remote)
            {
              this->state_ = stream_state::half_closed_local;
            }
            else if (this->state_ == stream_state::idle)
            {
              this->state_ = (incoming_headers_frame.has_end_stream_flag() ? stream_state::half_closed_remote : stream_state::open );
            }
            else
            {
              if (incoming_headers_frame.has_end_stream_flag())
                this->state_ = (this->state_ == stream_state::half_closed_local ? stream_state::closed : stream_state::half_closed_remote);
            }

            RecvMsg generic_head(std::move(headers));
            if (incoming_header_is_informational(generic_head))
              this->on_informational_headers_ ? this->on_informational_headers_(std::move(generic_head)) : void();
            else if (!this->on_headers_called_)
            {
              this->on_headers_called_ = true;
              this->on_headers_ ? this->on_headers_(std::move(generic_head)) : void();
            }
            else
              this->on_trailers_ ? this->on_trailers_(std::move(generic_head)) : void();

            if (this->state_ == stream_state::closed)
            {
              this->on_end_ ? this->on_end_() : void();
              this->on_close_ ? this->on_close_(v2_errc::no_error) : void();
            }
            else if (this->state_ == stream_state::half_closed_remote)
            {
              this->on_end_ ? this->on_end_() : void();
            }
          }
          break;
        }
        default:
        {
          // TODO: deal with frame / state_ mismatch
        }
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::handle_incoming_frame(const priority_frame& incoming_priority_frame, v2_errc& connection_error)
    {
      // TODO: implement.
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::handle_incoming_frame(const rst_stream_frame& incoming_rst_stream_frame, v2_errc& connection_error)
    {
      if (this->state_ != stream_state::closed)
      {
        std::queue<data_frame> rmv;
        this->outgoing_data_frames_.swap(rmv);
        this->state_= stream_state::closed;
        this->on_close_ ? this->on_close_(int_to_v2_errc(incoming_rst_stream_frame.error_code())) : void();
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::handle_incoming_frame(const settings_frame& incoming_settings_frame)
    {
      if (incoming_settings_frame.has_ack_flag())
      {
        if (this->pending_local_settings_.empty())
        {
          this->close(v2_errc::protocol_error);
        }
        else
        {
          auto& settings_list = this->pending_local_settings_.front();
          for (auto it = settings_list.begin(); it != settings_list.end(); ++it)
          {
            switch (it->first)
            {
              case (std::uint16_t)setting_code::header_table_size:
                this->local_settings_[setting_code::header_table_size] = it->second;
                break;
              case (std::uint16_t)setting_code::enable_push:
                this->local_settings_[setting_code::enable_push] = it->second;
                break;
              case (std::uint16_t)setting_code::max_concurrent_streams:
                this->local_settings_[setting_code::max_concurrent_streams] = it->second;
                break;
              case (std::uint16_t)setting_code::initial_window_size:
                if (it->second > 0x7FFFFFFF)
                {
                  this->close(v2_errc::flow_control_error);
                }
                else
                {
                  for (auto s = this->streams_.begin(); s != this->streams_.end(); ++s)
                  {
                    if (!s->second.adjust_local_window_size(static_cast<std::int32_t>(it->second) - static_cast<std::int32_t>(this->local_settings_[setting_code::initial_window_size])))
                    {
                      this->close(v2_errc::flow_control_error);
                      break;
                    }
                  }
                }
                this->local_settings_[setting_code::initial_window_size] = it->second;
                break;
              case (std::uint16_t)setting_code::max_frame_size:
                this->local_settings_[setting_code::max_frame_size] = it->second;
                break;
              case (std::uint16_t)setting_code::max_header_list_size:
                this->local_settings_[setting_code::max_header_list_size] = it->second;
                break;
            }
          }
        }
      }
      else
      {
        std::list<std::pair<std::uint16_t, std::uint32_t>> settings_list(incoming_settings_frame.settings());
        std::cout << settings_list.size() << std::endl;
        for (auto it = settings_list.begin(); it != settings_list.end(); ++it)
        {
          switch (it->first)
          {
            case (std::uint16_t)setting_code::header_table_size:
              this->peer_settings_[setting_code::header_table_size] = it->second;
              if (it->second < 4096)
                this->hpack_encoder_.add_table_size_update(it->second);
              else
                this->hpack_encoder_.add_table_size_update(4096);
              break;
            case (std::uint16_t)setting_code::enable_push:
              this->peer_settings_[setting_code::enable_push] = it->second;
              break;
            case (std::uint16_t)setting_code::max_concurrent_streams:
              this->peer_settings_[setting_code::max_concurrent_streams] = it->second;
              break;
            case (std::uint16_t)setting_code::initial_window_size:
              if (it->second > 0x7FFFFFFF)
              {
                this->close(v2_errc::flow_control_error);
              }
              else
              {
                for (auto s = this->streams_.begin(); s != this->streams_.end(); ++s)
                {
                  if (!s->second.adjust_peer_window_size(static_cast<std::int32_t>(it->second) - static_cast<std::int32_t>(this->peer_settings_[setting_code::initial_window_size])))
                  {
                    this->close(v2_errc::flow_control_error);
                    break;
                  }
                }
              }
              this->peer_settings_[setting_code::initial_window_size] = it->second;
              break;
            case (std::uint16_t)setting_code::max_frame_size:
              this->peer_settings_[setting_code::max_frame_size] = it->second;
              break;
            case (std::uint16_t)setting_code::max_header_list_size:
              this->peer_settings_[setting_code::max_header_list_size] = it->second;
              break;
          }
        }
        this->outgoing_frames_.push(http::frame(http::settings_frame(ack_flag()), 0x0));
      }
    }
    //----------------------------------------------------------------//

    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::handle_incoming_frame(const push_promise_frame& incoming_push_promise_frame, const std::vector<continuation_frame>& continuation_frames, hpack::decoder& dec, stream& idle_promised_stream, v2_errc& connection_error)
    {
      switch (this->state_)
      {
        case stream_state::half_closed_local:
        case stream_state::open:
        {
          std::string serialized_header_block;
          std::size_t serialized_header_block_sz = incoming_push_promise_frame.header_block_fragment_length();
          for (auto it = continuation_frames.begin(); it != continuation_frames.end(); ++it)
            serialized_header_block_sz += it->header_block_fragment_length();

          serialized_header_block.reserve(serialized_header_block_sz);

          serialized_header_block.append(incoming_push_promise_frame.header_block_fragment(), incoming_push_promise_frame.header_block_fragment_length());

          for (auto it = continuation_frames.begin(); it != continuation_frames.end(); ++it)
          {
            serialized_header_block.append(it->header_block_fragment(), it->header_block_fragment_length());
          }

          v2_header_block headers;
          if (!v2_header_block::deserialize(dec, serialized_header_block, headers))
            connection_error = v2_errc::compression_error;
          else
          {
            idle_promised_stream.state_ = stream_state::reserved_remote;
            SendMsg generic_head(std::move(headers));
            this->on_push_promise_ ? this->on_push_promise_(std::move(generic_head), incoming_push_promise_frame.promised_stream_id()) : void();
          }

          break;
        }
        case stream_state::closed:
        {
          if (true) // if stream was reset by me
          {
            if (true) //incoming_push_promise_frame.promised_stream_id() <= this->parent_connection_.last_newly_accepted_stream_id_)
            {
              //              this->parent_connection_.last_newly_accepted_stream_id_ = incoming_push_promise_frame.promised_stream_id();
              //              assert(this->parent_connection_.create_stream(this->parent_connection_.last_newly_accepted_stream_id_));
              //              auto it = this->parent_connection_.streams_.find(this->parent_connection_.last_newly_accepted_stream_id_);
              //              assert(it != this->parent_connection_.streams_.end());
              idle_promised_stream.state_ = stream_state::reserved_remote;
              idle_promised_stream.send_rst_stream_frame(v2_errc::refused_stream);
              //this->parent_connection_.send_reset_stream(this->parent_connection_.last_newly_accepted_stream_id_, errc::refused_stream);
            }
            else
            {
              // TODO: error promised stream_id is too low
            }
          }
          else
          {
            // TODO: handle error.
          }
          break;
        }
        default:
        {
          // TODO: deal with frame / state_ mismatch
        }
      }
    }

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::handle_incoming_frame(const ping_frame& incoming_ping_frame)
    {
      this->send_ping_acknowledgement(incoming_ping_frame.data());
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::handle_incoming_frame(const goaway_frame& incoming_goaway_frame)
    {
      auto c = incoming_goaway_frame.error_code();
      std::string s(incoming_goaway_frame.additional_debug_data(), incoming_goaway_frame.additional_debug_data_length());
      auto sz = s.size();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::handle_incoming_frame(const window_update_frame& incoming_window_update_frame)
    {
      std::int32_t amount = static_cast<std::int32_t>(incoming_window_update_frame.window_size_increment());
      if (amount > 0 && (amount + this->outgoing_window_size_) < this->outgoing_window_size_)
        this->close(v2_errc::flow_control_error);
      else
        this->outgoing_window_size_ = (amount + this->outgoing_window_size_);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::stream::handle_incoming_frame(const window_update_frame& incoming_window_update_frame, v2_errc& connection_error)
    {
      if (!this->adjust_peer_window_size(incoming_window_update_frame.window_size_increment()))
        this->send_rst_stream_frame(v2_errc::flow_control_error);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::send_data_frame(const char*const data, std::uint32_t data_sz, bool end_stream, std::uint32_t max_frame_size)
    {
      // TODO: Check max_frame_size
      switch (this->state_)
      {
        case stream_state::open:
          this->outgoing_data_frames_.push(http::data_frame(data, data_sz, end_stream));
          if (end_stream)
            this->state_ = stream_state::half_closed_local;
          return true;
        case stream_state::half_closed_remote:
          this->outgoing_data_frames_.push(http::data_frame(data, data_sz, end_stream));
          if (end_stream)
          {
            this->state_ = stream_state::closed;
            this->on_close_ ? this->on_close_(v2_errc::no_error) : void();
          }
          return true;
        case stream_state::reserved_remote:
        case stream_state::idle:
        case stream_state::reserved_local:
        case stream_state::half_closed_local:
        case stream_state::closed:
          return false;
      }

      return false;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::send_headers_frame(const v2_header_block& headers, bool end_stream, hpack::encoder& enc, std::uint32_t max_frame_size)
    {
      switch (this->state_)
      {
        case stream_state::idle:
        case stream_state::reserved_local:
        case stream_state::open:
        case stream_state::half_closed_remote:
        {
          std::string header_data;
          v2_header_block::serialize(enc, headers, header_data);

          const std::uint8_t EXTRA_BYTE_LENGTH_NEEDED_FOR_HEADERS_FRAME = 0; //TODO: Set correct value
          if ((header_data.size() + EXTRA_BYTE_LENGTH_NEEDED_FOR_HEADERS_FRAME) > max_frame_size)
          {
            assert("impl");
            // TODO: Split header into continuation frames.
          }
          else
          {
            this->outgoing_non_data_frames_.push(http::frame(http::headers_frame(header_data.data(), (std::uint32_t) header_data.size(), true, end_stream), this->id_));
          }

          switch (this->state_)
          {
            case stream_state::idle:
              this->state_ = stream_state::open;
              break;
            case stream_state::reserved_local:
              this->state_ = stream_state::half_closed_remote;
              break;
            case stream_state::open:
              if (end_stream)
                this->state_ = stream_state::half_closed_local;
              break;
            case stream_state::half_closed_remote:
              if (end_stream)
              {
                this->state_ = stream_state::closed;
                this->on_close_ ? this->on_close_(std::error_code()) : void();
              }
              break;
            default:
              break;
          }

          return true;
        }
        case stream_state::reserved_remote:
        case stream_state::half_closed_local:
        case stream_state::closed:
          return false;
      }

      return false;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::handle_outgoing_priority_frame_state_change()
    {
      switch (this->state_)
      {
        default:
          return true;
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::send_rst_stream_frame(v2_errc ec)
    {
      switch (this->state_)
      {
        case stream_state::idle:
        case stream_state::closed:
          return false;
        default:
          this->outgoing_non_data_frames_.push(http::frame(http::rst_stream_frame(ec), this->id_));
          std::queue<data_frame> rmv;
          this->outgoing_data_frames_.swap(rmv);
          this->state_ = stream_state::closed;
          this->on_close_ ? this->on_close_(ec) : void();
          return true;
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::send_push_promise_frame(const v2_header_block& headers, stream& promised_stream, hpack::encoder& enc, std::uint32_t max_frame_size)
    {
      switch (this->state_)
      {
        case stream_state::open:
        case stream_state::half_closed_remote:
        {
          std::string header_data;
          v2_header_block::serialize(enc, headers, header_data);

          const std::uint8_t EXTRA_BYTE_LENGTH_NEEDED_FOR_HEADERS_FRAME = 0; //TODO: Set correct value
          if ((header_data.size() + EXTRA_BYTE_LENGTH_NEEDED_FOR_HEADERS_FRAME) > max_frame_size)
          {
            assert("impl");
            // TODO: Split header into continuation frames.
          }
          else
          {
            this->outgoing_non_data_frames_.push(http::frame(http::push_promise_frame(header_data.data(), (std::uint32_t)header_data.size(), promised_stream.id(), true), this->id_));
          }

          promised_stream.state_ = stream_state::reserved_local;
          return true;
        }
        default:
          return false;
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::stream::send_window_update_frame(std::int32_t amount)
    {
      switch (this->state_)
      {
        case stream_state::idle:
        case stream_state::closed:
        case stream_state::reserved_local:
          return false;
        default:
          std::cout << "stream_wu:" << amount << std::endl;
          this->outgoing_non_data_frames_.push(http::frame(http::window_update_frame(amount), this->id_));
          this->incoming_window_size_ += amount;
          return true;
      }
    }
    //----------------------------------------------------------------//

//    //----------------------------------------------------------------//
//    template <typename SendMsg, typename RecvMsg>
//    bool v2_connection<SendMsg, RecvMsg>::stream::handle_outgoing_headers_state_change()
//    {
//      switch (this->state_)
//      {
//        case stream_state::idle:
//          this->state_ = stream_state::open;
//          return true;
//        case stream_state::reserved_local:
//          this->state_ = stream_state::half_closed_remote;
//          return true;
//        case stream_state::reserved_remote:
//        case stream_state::half_closed_local:
//        case stream_state::closed:
//          return false;
//        case stream_state::open:
//        case stream_state::half_closed_remote:
//          return true;
//      }
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    template <typename SendMsg, typename RecvMsg>
//    bool v2_connection<SendMsg, RecvMsg>::stream::handle_outgoing_end_stream_state_change()
//    {
//      switch (this->state_)
//      {
//        case stream_state::open:
//          this->state_ = stream_state::half_closed_local;
//          return true;
//        case stream_state::half_closed_remote:
//          this->state_ = stream_state::closed;
//          this->on_close_ ? this->on_close_(v2_errc::no_error) : void();
//          return true;
//        case stream_state::reserved_remote:
//        case stream_state::idle:
//        case stream_state::reserved_local:
//        case stream_state::half_closed_local:
//        case stream_state::closed:
//          return false;
//      }
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    template <typename SendMsg, typename RecvMsg>
//    bool v2_connection<SendMsg, RecvMsg>::stream::handle_outgoing_push_promise_state_change()
//    {
//      switch (this->state_)
//      {
//        case stream_state::idle:
//          this->state_ = stream_state::reserved_local;
//          return true;
//        default:
//          return false;
//      }
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    template <typename SendMsg, typename RecvMsg>
//    bool v2_connection<SendMsg, RecvMsg>::stream::handle_outgoing_rst_stream_state_change(errc ec)
//    {
//      switch (this->state_)
//      {
//        case stream_state::idle:
//        case stream_state::closed:
//          return false;
//        default:
//          this->state_ = stream_state::closed;
//          this->on_close_ ? this->on_close_(ec) : void();
//          return true;
//      }
//    }
//    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::run_send_loop()
    {
      if (!this->send_loop_running_)
      {
        this->send_loop_running_ = true;

        auto self = casted_shared_from_this();


        bool outgoing_frame_set = false;

        if (this->closed_)
        {
          while (this->outgoing_frames_.size())
          {
            if (!outgoing_frame_set && this->outgoing_frames_.front().type() == frame_type::goaway)
            {
              this->outgoing_frame_ = std::move(this->outgoing_frames_.front());
              outgoing_frame_set = true;
              frame::send_frame(*this->socket_, this->outgoing_frame_, [self](const std::error_code& ec)
              {
                self->socket_->close();
#ifndef MANIFOLD_REMOVED_PRIORITY
                self->stream_dependency_tree_.clear_children();
#endif
                self->streams_.clear();
                self->on_new_stream_ = nullptr;
                self->on_close_ = nullptr;
              });
            }
            this->outgoing_frames_.pop();
          }
        }
        else
        {
          if (this->outgoing_frames_.size())
          {
            this->outgoing_frame_ = std::move(this->outgoing_frames_.front());
            this->outgoing_frames_.pop();
            outgoing_frame_set = true;
          }
          else if (this->outgoing_window_size_)
          {
            auto stream_it = this->find_stream_with_data();
            if (stream_it != this->streams_.end())
            {
              this->outgoing_frame_ = std::move(stream_it->second.pop_data_frame(this->outgoing_window_size_));
              this->outgoing_window_size_ -= this->outgoing_frame_.data_frame().data_length();
              outgoing_frame_set = true;
            }
#ifndef MANIFOLD_REMOVED_PRIORITY
            stream* prioritized_stream_ptr = this->stream_dependency_tree_.get_next_send_stream_ptr(this->outgoing_window_size_, this->rng_);

            if (prioritized_stream_ptr)
            {
              this->outgoing_frame_ = std::move(prioritized_stream_ptr->pop_next_outgoing_frame(this->outgoing_window_size_));
              if (this->outgoing_frame_.type() == frame_type::data)
                this->outgoing_window_size_ -= this->outgoing_frame_.data_frame().data_length();
              outgoing_frame_set = true;
            }
#endif
          }

          if (outgoing_frame_set)
          {
            this->data_transfer_deadline_timer_.expires_from_now(this->data_transfer_timeout_);
            frame::send_frame(*this->socket_, this->outgoing_frame_, [self](const std::error_code& ec)
            {
              self->garbage_collect_streams();
              self->send_loop_running_ = false;
              if (ec)
              {
                self->close(v2_errc::internal_error);
                std::cout << "ERROR " << __FILE__ << ":" << __LINE__ << " " << ec.message() << std::endl;
              }
              else
              {
                self->run_send_loop();
              }
            });
          }
          else
          {
            // All pending frames are sent so cleanup.
            //this->garbage_collect_streams();
            this->send_loop_running_ = false;
          }
        }
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::run(std::chrono::system_clock::duration timeout, const std::list<std::pair<setting_code, std::uint32_t>>& custom_settings)
    {
      if (!this->started_)
      {
        this->data_transfer_timeout_ = timeout;
        this->data_transfer_deadline_timer_.expires_from_now(this->data_transfer_timeout_);
        this->run_timeout_loop();

        std::list<std::pair<std::uint16_t,std::uint32_t>> settings;
        for (auto it = custom_settings.begin(); it != custom_settings.end(); ++it)
        {
          if ( (it->first == setting_code::header_table_size && it->second != default_header_table_size)
            || (it->first == setting_code::enable_push && it->second != default_enable_push )
            || (it->first == setting_code::initial_window_size && it->second != default_initial_window_size )
            || (it->first == setting_code::max_frame_size && it->second != default_max_frame_size))
          {
            settings.emplace_back(static_cast<std::uint16_t>(it->first), it->second);
          }
        }

        this->send_settings(settings);
        this->run_recv_loop();

        this->started_ = true;
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_new_stream(const std::function<void(std::uint32_t)>& fn)
    {
      this->on_new_stream_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_close(const std::function<void(const std::error_code&)>& fn)
    {
      this->on_close_ = fn;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_data(std::uint32_t stream_id, const std::function<void(const char* const, std::size_t)>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_data(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_headers(std::uint32_t stream_id, const std::function<void(RecvMsg&&)>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_headers(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_informational_headers(std::uint32_t stream_id, const std::function<void(RecvMsg&&)>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_informational_headers(fn);
      }
    }
    //----------------------------------------------------------------//
#ifndef MANIFOLD_REMOVED_TRAILERS
    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_trailers(std::uint32_t stream_id, const std::function<void(header_block&&)>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_trailers(fn);
      }
    }
    //----------------------------------------------------------------//
#endif
    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_close(std::uint32_t stream_id, const std::function<void(const std::error_code&)>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_close(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_push_promise(std::uint32_t stream_id, const std::function<void(SendMsg&&, std::uint32_t)>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_push_promise(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_end(std::uint32_t stream_id, const std::function<void()>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_end(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::on_drain(std::uint32_t stream_id, const std::function<void()>& fn)
    {
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        it->second.on_drain(fn);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    std::uint32_t v2_connection<SendMsg, RecvMsg>::create_stream(std::uint32_t dependency_stream_id, std::uint32_t stream_id) //TODO: allow for dependency other than root.
    {
      std::uint32_t ret = 0;

      //std::unique_ptr<v2_connection<SendMsg, RecvMsg>::stream> s(this->create_stream_object(stream_id));

      if (stream_id == 0)
        stream_id = this->get_next_stream_id();
      if (stream_id)
      {
        auto insert_res = this->streams_.emplace(stream_id, stream(stream_id, this->outgoing_frames_, this->local_settings().at(setting_code::initial_window_size), this->peer_settings().at(setting_code::initial_window_size)));
        if (insert_res.second)
        {
#ifndef MANIFOLD_REMOVED_PRIORITY
          this->stream_dependency_tree_.insert_child(stream_dependency_tree_child_node(&(insert_res.first->second)));
#endif
          ret = insert_res.first->first;
        }
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    std::uint32_t v2_connection<SendMsg, RecvMsg>::get_next_stream_id()
    {
      std::uint32_t ret = 0;
      if (this->next_stream_id_ <= max_stream_id)
      {
        ret = this->next_stream_id_;
        this->next_stream_id_ += 2;
      }
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <>
    bool v2_connection<request_head, response_head>::send_headers(std::uint32_t stream_id, const request_head& head, bool end_headers, bool end_stream)
    {
      return this->send_headers(stream_id, v2_request_head(head), end_headers, end_stream);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <>
    bool v2_connection<response_head, request_head>::send_headers(std::uint32_t stream_id, const response_head& head, bool end_headers, bool end_stream)
    {
      return this->send_headers(stream_id, v2_response_head(head), end_headers, end_stream);
    }
    //----------------------------------------------------------------//
#ifndef MANIFOLD_REMOVED_TRAILERS
    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_trailers(std::uint32_t stream_id, const header_block& head, bool end_headers, bool end_stream)
    {
      return this->send_headers(stream_id, v2_header_block(head), end_headers, end_stream);
    }
    //----------------------------------------------------------------//
#endif
    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_headers(std::uint32_t stream_id, v2_header_block&& head, bool end_headers, bool end_stream)
    {
      bool ret = false;

      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        if (!it->second.send_headers_frame(head, end_stream, this->hpack_encoder_, this->peer_settings_[setting_code::max_frame_size]))
        {
          //assert(!"Stream state change not allowed.");
        }
        else
        {

          this->run_send_loop();
          ret = true;
        }
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_headers(std::uint32_t stream_id, v2_header_block&& head, priority_options priority, bool end_headers, bool end_stream)
    {
      bool ret = false;
#ifndef MANIFOLD_REMOVED_PRIORITY
      auto it = this->streams_.find(stream_id);

      if (it == this->streams_.end())
      {
        // TODO: Handle error
      }
      else
      {
        if (!it->second.handle_outgoing_headers_frame_state_change(end_stream))
        {
          assert(!"Stream state change not allowed.");
        }
        else
        {
          std::string header_data;
          v2_header_block::serialize(this->hpack_encoder_, head, header_data);
          const std::uint8_t EXTRA_BYTE_LENGTH_NEEDED_FOR_HEADERS_FRAME = 0; //TODO: Set correct value
          if ((header_data.size() + EXTRA_BYTE_LENGTH_NEEDED_FOR_HEADERS_FRAME) > this->peer_settings_[setting_code::max_frame_size])
          {
            // TODO: Split header into continuation frames.
          }

          this->outgoing_frames_.push(http::frame(http::headers_frame(header_data.data(), (std::uint32_t)header_data.size(), end_headers, end_stream, priority), stream_id));
          this->run_send_loop();
          ret = true;
        }
      }
#endif
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_priority(std::uint32_t stream_id, priority_options options)
    {
      bool ret = false;
#ifndef MANIFOLD_REMOVED_PRIORITY
      auto it = this->streams_.find(stream_id);
      if (it == this->streams_.end())
      {
        it->second.outgoing_non_data_frames.push(http::frame(http::priority_frame(options), stream_id));
      }
#endif
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::send_reset_stream(std::uint32_t stream_id, http::v2_errc error_code)
    {
      auto self = casted_shared_from_this();
      this->socket_->io_service().post([self, stream_id, error_code]()
      {
        auto it = self->streams_.find(stream_id);
        if (it != self->streams_.end())
        {
          if (it->second.send_rst_stream_frame(error_code))
          {
            self->run_send_loop();
          }
          else
          {
            //assert(!"Stream state_ change not allowed.");
          }
        }
      });
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::send_settings(const std::list<std::pair<std::uint16_t,std::uint32_t>>& settings)
    {
      this->pending_local_settings_.push(settings);
      this->outgoing_frames_.push(http::frame(http::settings_frame(settings.begin(), settings.end()), 0x0));
      this->run_send_loop();
    }
    //----------------------------------------------------------------//

//    //----------------------------------------------------------------//
//    template <typename SendMsg, typename RecvMsg>
//    bool v2_connection<SendMsg, RecvMsg>::send_countinuation(std::uint32_t stream_id, const v2_header_block& head, bool end_headers)
//    {
//      bool ret = false;
//
//      auto it = this->streams_.find(stream_id);
//
//      if (it == this->streams_.end())
//      {
//        // TODO: Handle error
//      }
//      else
//      {
//        std::string header_data;
//        v2_header_block::serialize(this->hpack_encoder_, head, header_data);
//        if (header_data.size() > this->peer_settings_[setting_code::max_frame_size])
//        {
//          // TODO: Handle error
//        }
//        else
//        {
//          it->second.outgoing_non_data_frames.push(http::frame(http::continuation_frame(header_data.data(), (std::uint32_t)header_data.size(), end_headers), stream_id));
//          this->run_send_loop();
//          ret = true;
//        }
//      }
//
//      return ret;
//    }
//    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_data(std::uint32_t stream_id, const char *const data, std::uint32_t data_sz, bool end_stream)
    {
      bool ret = false;

      auto it = this->streams_.find(stream_id);

      if (it != this->streams_.end())
      {
        if (it->second.send_data_frame(data, data_sz, end_stream, this->peer_settings_[setting_code::max_frame_size]))
        {
          this->run_send_loop();
          ret = true;
        }
        else
        {
          //assert(!"Stream state change not allowed.");
        }
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <>
    std::uint32_t v2_connection<request_head, response_head>::send_push_promise(std::uint32_t stream_id, const response_head& head)
    {
      return 0;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <>
    std::uint32_t v2_connection<response_head, request_head>::send_push_promise(std::uint32_t stream_id, const request_head& head)
    {
      return this->send_push_promise(stream_id, v2_request_head(head));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    std::uint32_t v2_connection<SendMsg, RecvMsg>::send_push_promise(std::uint32_t stream_id, v2_header_block&& head)
    {
      std::uint32_t promised_stream_id = 0;

      if (this->sending_push_promise_is_allowed())
      {
        auto it = this->streams_.find(stream_id);

        if (it == this->streams_.end())
        {
          // TODO: Handle error
        }
        else
        {
          promised_stream_id = this->create_stream(stream_id, 0);

          if (!promised_stream_id)
          {
            // TODO: stream_id's exhausted.
          }
          else
          {
            auto promised_stream = this->streams_.find(promised_stream_id);

            if(!it->second.send_push_promise_frame(head, promised_stream->second, this->hpack_encoder_, this->peer_settings_[setting_code::max_frame_size]))
            {
              promised_stream_id = 0;
              this->streams_.erase(promised_stream);
            }
            else
            {
              this->run_send_loop();
            }
          }
        }
      }

      return promised_stream_id;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::send_ping(std::uint64_t opaque_data)
    {
      this->outgoing_frames_.push(http::frame(http::ping_frame(opaque_data), 0x0));
      this->run_send_loop();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::send_ping_acknowledgement(std::uint64_t opaque_data)
    {
      this->outgoing_frames_.push(http::frame(http::ping_frame(opaque_data, true), 0x0));
      this->run_send_loop();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::send_goaway(http::v2_errc error_code, const char *const data, std::uint32_t data_sz)
    {
      this->outgoing_frames_.push(http::frame(http::goaway_frame(this->last_newly_accepted_stream_id_, error_code, data, data_sz), 0x0));
      this->run_send_loop();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    bool v2_connection<SendMsg, RecvMsg>::send_window_update(std::uint32_t stream_id, std::uint32_t amount)
    {
      bool ret = false;

      auto it = this->streams_.find(stream_id);
      if (it != this->streams_.end())
      {
        if (it->second.send_window_update_frame(amount))
        {
          this->run_send_loop();
          ret = true;
        }
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template <typename SendMsg, typename RecvMsg>
    void v2_connection<SendMsg, RecvMsg>::send_connection_level_window_update(std::uint32_t amount)
    {
      std::cout << "conn_wu:" << amount << std::endl;
      this->outgoing_frames_.push(http::frame(http::window_update_frame(amount), 0x0));
      this->incoming_window_size_ += amount;
    }
    //----------------------------------------------------------------//

    template class v2_connection<request_head, response_head>;
    template class v2_connection<response_head, request_head>;

//    //----------------------------------------------------------------//
//    void connection::send(char* buf, std::size_t buf_size, const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& handler)
//    {
//      this->socket_.async_send(asio::buffer(buf, buf_size), handler);
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    void connection::recv(const char* buf, std::size_t buf_size, const std::function<void(const std::error_code& ec, std::size_t bytes_transferred)>& handler)
//    {
//      this->socket_.async_send(asio::buffer(buf, buf_size), handler);
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    void connection::recvMessageHead()
//    {
//      std::shared_ptr<connection> self = this->shared_from_this();
//
//
//      TCP::recvline(this->socket_, this->incomingHeadBuffer_.data(), this->incomingHeadBuffer_.size(), [self](const std::error_code& ec, std::size_t bytes_transferred)
//      {
//        if (ec)
//        {
//          std::cout << ec.message() << ":" __FILE__ << "/" << __LINE__ << std::endl;
//        }
//        else
//        {
//
//          http::request_head requestHead;
//          v2_header_block::deserialize(std::string(self->incomingHeadBuffer_.data(), bytes_transferred), requestHead);
//
//          std::cout << requestHead.url() << ":" __FILE__ << "/" << __LINE__ << std::endl;
//
//          this->requests_.emplace(std::make_shared<http::server::request>(requestHead, this->socket_));
//        }
//      }, "\r\n\r\n");
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    void connection::close()
//    {
//      this->socket_.close();
//      this->server_.httpSessions_.erase(std::shared_ptr<Session>(this));
//    }
//    //----------------------------------------------------------------//
  }
}

#endif //MANIFOLD_DISABLE_HTTP2