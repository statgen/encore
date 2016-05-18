#pragma once

#ifndef MANIFOLD_HTTP_FRAME_HPP
#define MANIFOLD_HTTP_FRAME_HPP


#include <vector>
#include <list>
#include <cstdint>

#include "socket.hpp"
#include "http_error_category.hpp"


#ifndef MANIFOLD_DISABLE_HTTP2
namespace manifold
{
  namespace http
  {
    //================================================================//
//    enum class errc : std::uint32_t // TODO: Make error_condition
//    {
//      no_error            = 0x0,
//      protocol_error      = 0x1,
//      internal_error      = 0x2,
//      flow_control_error  = 0x3,
//      settings_timeout    = 0x4,
//      stream_closed       = 0x5,
//      frame_size_error    = 0x6,
//      refused_stream      = 0x7,
//      cancel              = 0x8,
//      compression_error   = 0x9,
//      connect_error       = 0xa,
//      enhance_your_calm   = 0xb,
//      inadequate_security = 0xc,
//      http_1_1_required   = 0xd
//    };
    //================================================================//


    //================================================================//
    class frame_flag
    {
    public:
      static const std::uint8_t end_stream  = 0x01;
      static const std::uint8_t end_headers = 0x04;
      static const std::uint8_t padded      = 0x08;
      static const std::uint8_t priority    = 0x20;
    };
    //================================================================//

    //================================================================//
    class frame_payload_base
    {
    protected:
      std::vector<char> buf_;
      std::uint8_t flags_;
    public:
      frame_payload_base(frame_payload_base&& source)
        : buf_(std::move(source.buf_)), flags_(source.flags_) {}
      frame_payload_base(std::uint8_t flags) : flags_(flags) {}
      virtual ~frame_payload_base() {}
      frame_payload_base& operator=(frame_payload_base&& source)
      {
        if (&source != this)
        {
          this->buf_ = std::move(source.buf_);
          this->flags_ = source.flags_;
        }
        return *this;
      }

      std::uint8_t flags() const;
      std::uint32_t serialized_length() const;


      static void recv_frame_payload(socket& sock, frame_payload_base& destination, std::uint32_t payload_size, std::uint8_t flags, const std::function<void(const std::error_code& ec)>& cb);
      static void send_frame_payload(socket& sock, const frame_payload_base& source, const std::function<void(const std::error_code& ec)>& cb);
    };
    //================================================================//

    //================================================================//
    class data_frame : public frame_payload_base
    {
    public:
      data_frame(const char*const data, std::uint32_t datasz, bool end_stream = false, const char*const padding = nullptr, std::uint8_t paddingsz = 0);
      data_frame(data_frame&& source) : frame_payload_base(std::move(source)) {}
      data_frame& operator=(data_frame&& source)
      {
        frame_payload_base::operator=(std::move(source));
        return *this;
      }
      ~data_frame() {}

      data_frame split(std::uint32_t num_bytes);

      const char*const data() const;
      std::uint32_t data_length() const;
      const char*const padding() const;
      std::uint8_t pad_length() const;

      bool has_end_stream_flag() const { return this->flags_ & frame_flag::end_stream; }
      bool has_padded_flag() const  { return this->flags_ & frame_flag::padded; }
    private:
      data_frame() : frame_payload_base(0) {}
      friend class frame;
    };
    //================================================================//

    //================================================================//
    struct priority_options
    {
      std::uint32_t stream_dependency_id;
      std::uint8_t weight;
      bool exclusive;
      priority_options(std::uint32_t dependency_id, std::uint8_t priority_weight, bool dependency_is_exclusive)
      {
        this->stream_dependency_id = dependency_id;
        this->weight = priority_weight;
        this->exclusive = dependency_is_exclusive;
      }
    };
    //================================================================//

    //================================================================//
    class headers_frame : public frame_payload_base
    {
    private:
      std::uint8_t bytes_needed_for_pad_length() const;
      std::uint8_t bytes_needed_for_dependency_id_and_exclusive_flag() const;
      std::uint8_t bytes_needed_for_weight() const;
    public:
      headers_frame(const char*const header_block, std::uint32_t header_block_sz, bool end_headers, bool end_stream, const char*const padding = nullptr, std::uint8_t paddingsz = 0);
      headers_frame(const char*const header_block, std::uint32_t header_block_sz, bool end_headers, bool end_stream, priority_options priority_ops, const char*const padding = nullptr, std::uint8_t paddingsz = 0);
      headers_frame(headers_frame&& source) : frame_payload_base(std::move(source)) {}
      headers_frame& operator=(headers_frame&& source)
      {
        frame_payload_base::operator=(std::move(source));
        return *this;
      }
      ~headers_frame() {}

      const char*const header_block_fragment() const;
      std::uint32_t header_block_fragment_length() const;
      const char*const padding() const;
      std::uint8_t pad_length() const;
      std::uint8_t weight() const;
      std::uint32_t stream_dependency_id() const;
      bool exclusive_stream_dependency() const;

      bool has_end_stream_flag() const { return this->flags_ & frame_flag::end_stream; }
      bool has_end_headers_flag() const { return this->flags_ & frame_flag::end_headers; }
      bool has_padded_flag() const  { return this->flags_ & frame_flag::padded; }
      bool has_priority_flag() const { return this->flags_ & frame_flag::priority; }
    private:
      headers_frame() : frame_payload_base(0) {}
      friend class frame;
    };
    //================================================================//

    //================================================================//
    class priority_frame : public frame_payload_base
    {
    public:
      priority_frame(priority_options options);
      priority_frame(priority_frame&& source) : frame_payload_base(std::move(source)) {}
      priority_frame& operator=(priority_frame&& source)
      {
        frame_payload_base::operator=(std::move(source));
        return *this;
      }
      ~priority_frame() {}

      std::uint8_t weight() const;
      std::uint32_t stream_dependency_id() const;
      bool exclusive_stream_dependency() const;
    private:
      priority_frame() : frame_payload_base(0) {}
      friend class frame;
    };
    //================================================================//

    //================================================================//
    class rst_stream_frame : public frame_payload_base
    {
    public:
      rst_stream_frame(http::v2_errc error_code);
      rst_stream_frame(rst_stream_frame&& source) : frame_payload_base(std::move(source)) {}
      rst_stream_frame& operator=(rst_stream_frame&& source)
      {
        frame_payload_base::operator=(std::move(source));
        return *this;
      }
      ~rst_stream_frame() {}

      std::uint32_t error_code() const;
    private:
      rst_stream_frame() : frame_payload_base(0) {}
      friend class frame;
    };
    //================================================================//

    class ack_flag { };

    //================================================================//
    class settings_frame : public frame_payload_base
    {
    public:
      settings_frame(ack_flag) : frame_payload_base(0x1) {}
      settings_frame(std::list<std::pair<std::uint16_t,std::uint32_t>>::const_iterator beg, std::list<std::pair<std::uint16_t,std::uint32_t>>::const_iterator end);
      settings_frame(settings_frame&& source) : frame_payload_base(std::move(source)) {}
      settings_frame& operator=(settings_frame&& source)
      {
        frame_payload_base::operator=(std::move(source));
        return *this;
      }
      ~settings_frame() {}


      bool has_ack_flag() const { return (bool)(this->flags_ & 0x1); }
      std::list<std::pair<std::uint16_t,std::uint32_t>> settings() const;
    private:
      settings_frame() : frame_payload_base(0) {}
      friend class frame;
    };
    //================================================================//

    //================================================================//
    class push_promise_frame : public frame_payload_base // TODO: Impl optional padding. Also flags need to looked at!!
    {
    public:
      push_promise_frame(const char*const header_block, std::uint32_t header_block_sz, std::uint32_t promise_stream_id, bool end_headers, const char*const padding = nullptr, std::uint8_t paddingsz = 0);
      push_promise_frame(push_promise_frame&& source) : frame_payload_base(std::move(source)) {}
      push_promise_frame& operator=(push_promise_frame&& source)
      {
        frame_payload_base::operator=(std::move(source));
        return *this;
      }
      ~push_promise_frame() {}

      const char*const header_block_fragment() const;
      std::uint32_t header_block_fragment_length() const;
      const char*const padding() const;
      std::uint8_t pad_length() const;
      std::uint32_t promised_stream_id() const;
      bool has_end_headers_flag() const { return this->flags_ & frame_flag::end_headers; }
    private:
      push_promise_frame() : frame_payload_base(0) {}
      friend class frame;
    };
    //================================================================//

    //================================================================//
    class ping_frame : public frame_payload_base
    {
    public:
      ping_frame(std::uint64_t ping_data, bool ack = false);
      ping_frame(ping_frame&& source) : frame_payload_base(std::move(source)) {}
      ping_frame& operator=(ping_frame&& source)
      {
        frame_payload_base::operator=(std::move(source));
        return *this;
      }
      ~ping_frame() {}

      bool is_ack() const { return (bool)(this->flags_ & 0x1); }
      std::uint64_t data() const;
    private:
      ping_frame() : frame_payload_base(0) {}
      friend class frame;
    };
    //================================================================//

    //================================================================//
    class goaway_frame : public frame_payload_base
    {
    public:
      goaway_frame(std::uint32_t last_stream_id, http::v2_errc error_code, const char*const addl_error_data, std::uint32_t addl_error_data_sz);
      goaway_frame(goaway_frame&& source) : frame_payload_base(std::move(source)) {}
      goaway_frame& operator=(goaway_frame&& source)
      {
        frame_payload_base::operator=(std::move(source));
        return *this;
      }
      ~goaway_frame() {}

      std::uint32_t last_stream_id() const;
      http::v2_errc error_code() const;
      const char*const additional_debug_data() const;
      std::uint32_t additional_debug_data_length() const;
    private:
      goaway_frame() : frame_payload_base(0) {}
      friend class frame;
    };
    //================================================================//

    //================================================================//
    class window_update_frame : public frame_payload_base
    {
    public:
      window_update_frame(std::uint32_t window_size_increment);
      window_update_frame(window_update_frame&& source) : frame_payload_base(std::move(source)) {}
      window_update_frame& operator=(window_update_frame&& source)
      {
        frame_payload_base::operator=(std::move(source));
        return *this;
      }
      ~window_update_frame() {}

      std::uint32_t window_size_increment() const;
    private:
      window_update_frame() : frame_payload_base(0) {}
      friend class frame;
    };
    //================================================================//

    //================================================================//
    class continuation_frame : public frame_payload_base
    {
    public:
      continuation_frame(const char*const header_data, std::uint32_t header_data_sz, bool end_headers);
      continuation_frame(continuation_frame&& source) : frame_payload_base(std::move(source)) {}
      continuation_frame& operator=(continuation_frame&& source)
      {
        frame_payload_base::operator=(std::move(source));
        return *this;
      }
      ~continuation_frame() {}

      const char*const header_block_fragment() const;
      std::uint32_t header_block_fragment_length() const;

      bool has_end_headers_flag() const { return this->flags_ & frame_flag::end_headers; }
    private:
      continuation_frame() : frame_payload_base(0) {}
      friend class frame;
    };
    //================================================================//

    //================================================================//
    enum class frame_type : std::uint8_t
    {
      data = 0x0,
      headers,
      priority,
      rst_stream,
      settings,
      push_promise,
      ping,
      goaway,
      window_update,
      continuation,
      invalid_type = 0xFF
    };
    //================================================================//

    //================================================================//
    class frame
    {
    public:
      //----------------------------------------------------------------//
      static const http::data_frame          default_data_frame_         ;
      static const http::headers_frame       default_headers_frame_      ;
      static const http::priority_frame      default_priority_frame_     ;
      static const http::rst_stream_frame    default_rst_stream_frame_   ;
      static const http::settings_frame      default_settings_frame_     ;
      static const http::push_promise_frame  default_push_promise_frame_ ;
      static const http::ping_frame          default_ping_frame_         ;
      static const http::goaway_frame        default_goaway_frame_       ;
      static const http::window_update_frame default_window_update_frame_;
      static const http::continuation_frame  default_continuation_frame_ ;
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      static void recv_frame(manifold::socket& sock, frame& destination, const std::function<void(const std::error_code& ec)>& cb);
      static void send_frame(manifold::socket& sock, const frame& source, const std::function<void(const std::error_code& ec)>& cb);
      //----------------------------------------------------------------//
    private:
      //----------------------------------------------------------------//
      union payload_union
      {
        //----------------------------------------------------------------//
        http::data_frame          data_frame_;
        http::headers_frame       headers_frame_;
        http::priority_frame      priority_frame_;
        http::rst_stream_frame    rst_stream_frame_;
        http::settings_frame      settings_frame_;
        http::push_promise_frame  push_promise_frame_;
        http::ping_frame          ping_frame_;
        http::goaway_frame        goaway_frame_;
        http::window_update_frame window_update_frame_;
        http::continuation_frame  continuation_frame_;
        //----------------------------------------------------------------//

        //----------------------------------------------------------------//
        payload_union(){}
        ~payload_union(){}
        //----------------------------------------------------------------//
      };
      //----------------------------------------------------------------//
    private:
      //----------------------------------------------------------------//
      payload_union payload_;
      std::array<char, 9> metadata_;
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      void destroy_union();
      void init_meta(frame_type t, std::uint32_t payload_length, std::uint32_t stream_id, std::uint8_t flags);
      std::uint8_t flags() const;
      frame(const frame&) = delete;
      frame& operator=(const frame&) = delete;
      //----------------------------------------------------------------//
    public:
      //----------------------------------------------------------------//
      frame();
      frame(http::data_frame&& payload, std::uint32_t stream_id);
      frame(http::headers_frame&& payload, std::uint32_t stream_id);
      frame(http::priority_frame&& payload, std::uint32_t stream_id);
      frame(http::rst_stream_frame&& payload, std::uint32_t stream_id);
      frame(http::settings_frame&& payload, std::uint32_t stream_id);
      frame(http::push_promise_frame&& payload, std::uint32_t stream_id);
      frame(http::ping_frame&& payload, std::uint32_t stream_id);
      frame(http::goaway_frame&& payload, std::uint32_t stream_id);
      frame(http::window_update_frame&& payload, std::uint32_t stream_id);
      frame(http::continuation_frame&& payload, std::uint32_t stream_id);
      frame(frame&& source);
      ~frame();
      frame& operator=(frame&& source);
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      template <typename T>
      bool is() const;

      std::uint32_t payload_length() const;
      frame_type type() const;
      std::uint32_t stream_id() const;
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      http::data_frame&           data_frame()         ;
      http::headers_frame&        headers_frame()      ;
      http::priority_frame&       priority_frame()     ;
      http::rst_stream_frame&     rst_stream_frame()   ;
      http::settings_frame&       settings_frame()     ;
      http::push_promise_frame&   push_promise_frame() ;
      http::ping_frame&           ping_frame()         ;
      http::goaway_frame&         goaway_frame()       ;
      http::window_update_frame&  window_update_frame();
      http::continuation_frame&   continuation_frame() ;

      const http::data_frame&           data_frame()          const;
      const http::headers_frame&        headers_frame()       const;
      const http::priority_frame&       priority_frame()      const;
      const http::rst_stream_frame&     rst_stream_frame()    const;
      const http::settings_frame&       settings_frame()      const;
      const http::push_promise_frame&   push_promise_frame()  const;
      const http::ping_frame&           ping_frame()          const;
      const http::goaway_frame&         goaway_frame()        const;
      const http::window_update_frame&  window_update_frame() const;
      const http::continuation_frame&   continuation_frame()  const;
      //----------------------------------------------------------------//
    };
    //================================================================//
  }
}

#endif //MANIFOLD_DISABLE_HTTP2

#endif //MANIFOLD_HTTP_FRAME_HPP