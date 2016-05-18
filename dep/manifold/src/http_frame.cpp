#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <new>
#include <iostream>

#include "asio/ssl.hpp"
#include "http_frame.hpp"

#ifndef MANIFOLD_DISABLE_HTTP2
namespace manifold
{
  namespace http
  {
    enum class log_dir { outgoing = 1, incoming };
    void log(const frame& f, log_dir direction)
    {
#if 0
      std::cout << (direction == log_dir::outgoing ? "--- OUT ---" : "--- IN ---") << std::endl;
      std::cout << "Stream ID: " << f.stream_id() << std::endl;
      std::cout << "Payload Length: " << f.payload_length() << std::endl;

      std::string str_type;
      switch(f.type())
      {
        case frame_type::data           : str_type = "data" ; break;
        case frame_type::headers        : str_type = "headers" ; break;
        case frame_type::priority       : str_type = "priority" ; break;
        case frame_type::rst_stream     : str_type = "rst_stream" ; break;
        case frame_type::settings       : str_type = "settings" ; break;
        case frame_type::push_promise   : str_type = "push_promise" ; break;
        case frame_type::ping           : str_type = "ping" ; break;
        case frame_type::goaway         : str_type = "goaway" ; break;
        case frame_type::window_update  : str_type = "window_update" ; break;
        case frame_type::continuation   : str_type = "continuation" ; break;
        case frame_type::invalid_type   : str_type = "invalid" ; break;
      }
      std::cout << "Frame Type: " << str_type << std::endl;
      std::cout << std::endl;
#endif
    }
    //****************************************************************//
    // frame_payload_base
    //----------------------------------------------------------------//
    std::uint8_t frame_payload_base::flags() const
    {
      return this->flags_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t frame_payload_base::serialized_length() const
    {
      return (std::uint32_t)this->buf_.size();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void frame_payload_base::recv_frame_payload(socket& sock, frame_payload_base& destination, std::uint32_t payload_size, std::uint8_t flags, const std::function<void(const std::error_code& ec)>& cb)
    {
      destination.flags_ = flags;
      destination.buf_.resize(payload_size);
      sock.recv(destination.buf_.data(), payload_size, [cb](const std::error_code& ec, std::size_t bytes_read)
      {
        (cb ? cb(ec) : void());
      });
    }
    //template void frame_payload_base::recv_frame_payload<asio::ip::tcp::socket>(asio::ip::tcp::socket& sock, frame_payload_base& destination, std::uint32_t payload_size, std::uint8_t flags, const std::function<void(const std::error_code& ec)>& cb);
    //template void frame_payload_base::recv_frame_payload<asio::ssl::stream<asio::ip::tcp::socket>>(asio::ssl::stream<asio::ip::tcp::socket>& sock, frame_payload_base& destination, std::uint32_t payload_size, std::uint8_t flags, const std::function<void(const std::error_code& ec)>& cb);
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void frame_payload_base::send_frame_payload(socket& sock, const frame_payload_base& source, const std::function<void(const std::error_code& ec)>& cb)
    {
      sock.send(source.buf_.data(), source.buf_.size(), [cb](const std::error_code& ec, std::size_t bytes_transfered)
      {
        (cb ? cb(ec) : void());
      });
    }
    //template void frame_payload_base::send_frame_payload<asio::ip::tcp::socket>(asio::ip::tcp::socket& sock, const frame_payload_base& source, const std::function<void(const std::error_code& ec)>& cb);
    //template void frame_payload_base::send_frame_payload<asio::ssl::stream<asio::ip::tcp::socket>>(asio::ssl::stream<asio::ip::tcp::socket>& sock, const frame_payload_base& source, const std::function<void(const std::error_code& ec)>& cb);
    //----------------------------------------------------------------//
    //****************************************************************//



    //****************************************************************//
    // data_frame
    //----------------------------------------------------------------//
    data_frame::data_frame(const char*const data, std::uint32_t datasz, bool end_stream, const char*const padding, std::uint8_t paddingsz)
        : frame_payload_base((end_stream ? frame_flag::end_stream : (std::uint8_t)0x0) | (padding && paddingsz ? frame_flag::padded : (std::uint8_t)0x0))
    {
      if (this->flags_ & frame_flag::padded)
      {
        this->buf_.resize(datasz + 1 + paddingsz);
        this->buf_[0] = paddingsz;
        memcpy(this->buf_.data() + 1, data, datasz);
        memcpy(this->buf_.data() + 1 + datasz, padding, paddingsz);
      }
      else
      {
        this->buf_.resize(datasz);
        memcpy(this->buf_.data(), data, datasz);
      }


      memcpy(this->buf_.data(), data, datasz);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    data_frame data_frame::split(std::uint32_t num_bytes)
    {
      std::uint32_t this_data_length = this->data_length();
      if (num_bytes > this_data_length)
        num_bytes = this_data_length;


      data_frame ret(this->data(), num_bytes);
      data_frame::operator=(data_frame(this->data() + num_bytes, this->data_length() - num_bytes, this->flags_ & frame_flag::end_stream, this->padding(), this->pad_length()));
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const char*const data_frame::data() const
    {
      if (this->flags_ & frame_flag::padded)
        return this->buf_.data() + 1;
      else
        return this->buf_.data();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t data_frame::data_length() const
    {
      return (std::uint32_t)(this->buf_.size() - (this->pad_length() + (this->flags_ & frame_flag::padded ? 1 : 0)));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const char*const data_frame::padding() const
    {
      if (this->flags_ & frame_flag::padded)
        return this->buf_.data() + 1 + this->data_length();
      else
        return nullptr;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t data_frame::pad_length() const
    {
      std::uint8_t ret = 0;
      if (this->flags_ & frame_flag::padded)
        memcpy(&ret, this->buf_.data(), 1);
      return ret;
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // headers_frame
    //----------------------------------------------------------------//
    headers_frame::headers_frame(const char*const header_block, std::uint32_t header_block_sz, bool end_headers, bool end_stream, const char*const padding, std::uint8_t paddingsz)
        : frame_payload_base(
            (end_stream ? frame_flag::end_stream : (std::uint8_t)0x0)
            | (end_headers ? frame_flag::end_headers : (std::uint8_t)0x0)
            | (padding && paddingsz ? frame_flag::padded : (std::uint8_t)0x0))
    {
      this->buf_.resize(this->bytes_needed_for_pad_length() + header_block_sz + (this->bytes_needed_for_pad_length() ? paddingsz : 0));

      memcpy(this->buf_.data() + this->bytes_needed_for_pad_length(), header_block, header_block_sz);
      if (this->flags_ & frame_flag::padded)
      {
        this->buf_[0] = paddingsz;
        memcpy(this->buf_.data() + this->bytes_needed_for_pad_length() + header_block_sz, padding, paddingsz);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    headers_frame::headers_frame(const char*const header_block, std::uint32_t header_block_sz, bool end_headers, bool end_stream, priority_options priority_ops, const char*const padding, std::uint8_t paddingsz)
        : frame_payload_base(
        (end_stream ? frame_flag::end_stream : (std::uint8_t)0x0)
        | (end_headers ? frame_flag::end_headers : (std::uint8_t)0x0)
        | (frame_flag::priority)
        | (padding && paddingsz ? frame_flag::padded : (std::uint8_t)0x0))
    {
      this->buf_.resize(this->bytes_needed_for_pad_length() + 5 + header_block_sz + (this->bytes_needed_for_pad_length() ? paddingsz : 0));
      if (this->flags_ & frame_flag::padded)
      {
        this->buf_[0] = paddingsz;
        memcpy(this->buf_.data() + this->bytes_needed_for_pad_length() + 5 + header_block_sz, padding, paddingsz);
      }
      std::uint32_t tmp_nbo = htonl(priority_ops.exclusive ? (0x80000000 ^ priority_ops.stream_dependency_id) : (0x7FFFFFFF & priority_ops.stream_dependency_id));
      memcpy(this->buf_.data() + this->bytes_needed_for_pad_length(), &tmp_nbo, 4);
      memcpy(this->buf_.data() + this->bytes_needed_for_pad_length() + 4, &priority_ops.weight, 1);
      memcpy(this->buf_.data() + this->bytes_needed_for_pad_length() + 5, header_block, header_block_sz);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t headers_frame::bytes_needed_for_pad_length() const
    {
      return (std::uint8_t)(this->flags_ & frame_flag::padded ? 1 : 0);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t headers_frame::bytes_needed_for_dependency_id_and_exclusive_flag() const
    {
      return (std::uint8_t)(this->flags_ & frame_flag::priority ? 4 : 0);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t headers_frame::bytes_needed_for_weight() const
    {
      return (std::uint8_t)(this->flags_ & frame_flag::priority ? 1 : 0);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const char*const headers_frame::header_block_fragment() const
    {
      return (this->buf_.data() + this->bytes_needed_for_pad_length() + this->bytes_needed_for_dependency_id_and_exclusive_flag() + this->bytes_needed_for_weight());
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t headers_frame::header_block_fragment_length() const
    {
      return (std::uint32_t)(this->buf_.size() - (this->pad_length() + this->bytes_needed_for_pad_length() + this->bytes_needed_for_dependency_id_and_exclusive_flag() + this->bytes_needed_for_weight()));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const char*const headers_frame::padding() const
    {
      if (this->flags_ & frame_flag::padded)
        return this->buf_.data() + 1 + this->bytes_needed_for_dependency_id_and_exclusive_flag() + this->bytes_needed_for_weight() + this->header_block_fragment_length();
      else
        return nullptr;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t headers_frame::pad_length() const
    {
      std::uint8_t ret = 0;
      if (this->flags_ & frame_flag::padded)
        memcpy(&ret, this->buf_.data(), 1);
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t headers_frame::weight() const
    {
      std::uint8_t ret = 0;
      if (this->flags_ & frame_flag::priority)
        memcpy(&ret, this->buf_.data() + this->bytes_needed_for_pad_length() + 4, 1);
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t headers_frame::stream_dependency_id() const
    {
      std::uint32_t ret = 0;
      if (this->flags_ & frame_flag::priority)
        memcpy(&ret, this->buf_.data() + this->bytes_needed_for_pad_length(), 4);
      return (0x7FFFFFFF & ntohl(ret));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool headers_frame::exclusive_stream_dependency() const
    {
      std::uint8_t tmp = 0;
      if (this->flags_ & frame_flag::priority)
        memcpy(&tmp, this->buf_.data() + this->bytes_needed_for_pad_length(), 1);

      return (0x80 & tmp) != 0;
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // priority_frame
    //----------------------------------------------------------------//
    priority_frame::priority_frame(priority_options options) : frame_payload_base(0x0)
    {
      this->buf_.resize(5);
      std::uint32_t tmp = (options.exclusive ? (0x80000000 ^ options.stream_dependency_id) : (0x7FFFFFFF & options.stream_dependency_id));
      std::uint32_t tmp_nbo = htonl(tmp);
      memcpy(this->buf_.data(), &tmp_nbo, 4);
      memcpy(this->buf_.data() + 4, &(options.weight), 1);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t priority_frame::weight() const
    {
      std::uint8_t ret;
      memcpy(&ret, this->buf_.data() + 4, 1);
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t priority_frame::stream_dependency_id() const
    {
      std::uint32_t ret;
      memcpy(&ret, this->buf_.data(), 4);
      return (0x7FFFFFFF & ntohl(ret));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool priority_frame::exclusive_stream_dependency() const
    {
      std::uint8_t tmp;
      memcpy(&tmp, this->buf_.data(), 1);

      return (0x80 & tmp) != 0;
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // rst_stream_frame
    //----------------------------------------------------------------//
    rst_stream_frame::rst_stream_frame(http::v2_errc error_code) : frame_payload_base(0x0)
    {
      this->buf_.resize(4);
      std::uint32_t error_code_nbo = htonl((std::uint32_t)error_code);
      memcpy(this->buf_.data(), &error_code_nbo, 4);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t rst_stream_frame::error_code() const
    {
      std::uint32_t tmp;
      memcpy(&tmp, this->buf_.data(), 4);
      return ntohl(tmp);
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // settings_frame
    //----------------------------------------------------------------//
    settings_frame::settings_frame(std::list<std::pair<std::uint16_t,std::uint32_t>>::const_iterator beg, std::list<std::pair<std::uint16_t,std::uint32_t>>::const_iterator end)
      : frame_payload_base(0x0)
    {
      this->buf_.resize(6 * std::distance(beg, end));
      std::size_t pos = 0;
      for (auto it = beg; it != end; ++it)
      {
        std::uint16_t key_nbo(htons(it->first));
        std::uint32_t value_nbo(htonl(it->second));
        memcpy(&this->buf_[pos], &key_nbo,  2);
        memcpy(&this->buf_[pos + 2], &value_nbo,  4);
        pos = pos + 6;
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::list<std::pair<std::uint16_t,std::uint32_t>> settings_frame::settings() const
    {
      std::list<std::pair<std::uint16_t,std::uint32_t>> ret;

      std::size_t bytesToParse = this->buf_.size();
      std::size_t pos = 0;
      while (bytesToParse >= 6)
      {
        std::uint16_t key;
        std::uint32_t value;
        memcpy(&key, &this->buf_[pos], 2);
        memcpy(&value, &this->buf_[pos + 2], 4);
        ret.push_back(std::pair<std::uint16_t,std::uint32_t>(ntohs(key), ntohl(value)));
        pos = pos + 6;
        bytesToParse = bytesToParse - 6;
      }

      return ret;
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // push_promise_frame
    //----------------------------------------------------------------//
    push_promise_frame::push_promise_frame(const char*const header_block, std::uint32_t header_block_sz, std::uint32_t promise_stream_id, bool end_headers, const char*const padding, std::uint8_t paddingsz)
      : frame_payload_base((std::uint8_t)(end_headers ? frame_flag::end_headers : 0x0) | (std::uint8_t)(padding && paddingsz ? frame_flag::padded : 0))
    {
      if (this->flags_ & frame_flag::padded)
      {
        this->buf_.resize(5 + header_block_sz + paddingsz);
        this->buf_[0] = paddingsz;
        std::uint32_t promise_stream_id_nbo = htonl(0x7FFFFFFF & promise_stream_id);
        memcpy(this->buf_.data() + 1, &promise_stream_id_nbo, 4);
        memcpy(this->buf_.data() + 5, header_block, header_block_sz);
        memcpy(this->buf_.data() + 5 + header_block_sz, padding, paddingsz);
      }
      else
      {
        this->buf_.resize(4 + header_block_sz);
        std::uint32_t promise_stream_id_nbo = htonl(0x7FFFFFFF & promise_stream_id);
        memcpy(this->buf_.data(), &promise_stream_id_nbo, 4);
        memcpy(this->buf_.data() + 4, header_block, header_block_sz);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const char*const push_promise_frame::header_block_fragment() const
    {
      if (this->flags_ & frame_flag::padded)
        return (this->buf_.data() + 5);
      else
        return (this->buf_.data() + 4);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t push_promise_frame::header_block_fragment_length() const
    {
      if (this->flags_ & frame_flag::padded)
        return (std::uint32_t)(this->buf_.size() - (this->pad_length() + 5));
      else
        return (std::uint32_t)(this->buf_.size() - 4);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const char*const push_promise_frame::padding() const
    {
      if (this->flags_ & frame_flag::padded)
        return (this->buf_.data() + 5 + this->header_block_fragment_length());
      else
        return nullptr;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t push_promise_frame::pad_length() const
    {
      std::uint8_t ret = 0;
      if (this->flags_ & frame_flag::padded)
        memcpy(&ret, this->buf_.data(), 1);
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t push_promise_frame::promised_stream_id() const
    {
      std::uint32_t ret;
      memcpy(&ret, this->buf_.data() + (this->flags_ & frame_flag::padded ? 1 : 0), 4);
      return (0x7FFFFFFF & ntohl(ret));
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // ping_frame
    //----------------------------------------------------------------//
    ping_frame::ping_frame(std::uint64_t ping_data, bool ack) : frame_payload_base((std::uint8_t)(ack ? 0x1 : 0))
    {
      this->buf_.resize(8);
      std::uint64_t ping_data_nbo = htonll(ping_data);
      memcpy(this->buf_.data(), &ping_data_nbo, 8);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint64_t ping_frame::data() const
    {
      std::uint64_t ret;
      memcpy(&ret, this->buf_.data(), 8);
      return ntohll(ret);
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // goaway_frame
    //----------------------------------------------------------------//
    goaway_frame::goaway_frame(std::uint32_t last_stream_id, http::v2_errc error_code, const char*const addl_error_data, std::uint32_t addl_error_data_sz) : frame_payload_base(0)
    {
      this->buf_.resize(8 + addl_error_data_sz);
      std::uint32_t tmp_nbo = htonl(0x7FFFFFFF & last_stream_id);
      std::uint32_t error_code_nbo = htonl((std::uint32_t)error_code);
      memcpy(this->buf_.data(), &tmp_nbo, 4);
      memcpy(this->buf_.data() + 4, &error_code_nbo, 4);
      memcpy(this->buf_.data() + 8, addl_error_data, addl_error_data_sz);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t goaway_frame::last_stream_id() const
    {
      std::uint32_t ret;
      memcpy(&ret, this->buf_.data(), 4);
      return (0x7FFFFFFF & ntohl(ret));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    http::v2_errc goaway_frame::error_code() const
    {
      std::uint32_t tmp;
      memcpy(&tmp, this->buf_.data() + 4, 4);
      return int_to_v2_errc(ntohl(tmp));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const char*const goaway_frame::additional_debug_data() const
    {
      return (this->buf_.data() + 8);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t goaway_frame::additional_debug_data_length() const
    {
      return (std::uint32_t)(this->buf_.size() - 8);
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // window_update_frame
    //----------------------------------------------------------------//
    window_update_frame::window_update_frame(std::uint32_t window_size_increment) : frame_payload_base(0)
    {
      this->buf_.resize(4);
      std::uint32_t tmp_nbo = htonl(0x7FFFFFFF & window_size_increment);
      memcpy(this->buf_.data(), &tmp_nbo, 4);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t window_update_frame::window_size_increment() const
    {
      std::uint32_t ret;
      memcpy(&ret, this->buf_.data(), 4);
      return (0x7FFFFFFF & ntohl(ret));
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // continuation_frame
    //----------------------------------------------------------------//
    continuation_frame::continuation_frame(const char*const header_data, std::uint32_t header_data_sz, bool end_headers) : frame_payload_base(end_headers ? frame_flag::end_headers : (std::uint8_t)0)
    {
      this->buf_.resize(header_data_sz);
      memcpy(this->buf_.data(), header_data, header_data_sz);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const char*const continuation_frame::header_block_fragment() const
    {
      return this->buf_.data();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t continuation_frame::header_block_fragment_length() const
    {
      return (std::uint32_t)this->buf_.size();
    }
    //----------------------------------------------------------------//
    //****************************************************************//

    //****************************************************************//
    // frame
    //----------------------------------------------------------------//
    const http::data_frame          frame::default_data_frame_         ;
    const http::headers_frame       frame::default_headers_frame_      ;
    const http::priority_frame      frame::default_priority_frame_     ;
    const http::rst_stream_frame    frame::default_rst_stream_frame_   ;
    const http::settings_frame      frame::default_settings_frame_     ;
    const http::push_promise_frame  frame::default_push_promise_frame_ ;
    const http::ping_frame          frame::default_ping_frame_         ;
    const http::goaway_frame        frame::default_goaway_frame_       ;
    const http::window_update_frame frame::default_window_update_frame_;
    const http::continuation_frame  frame::default_continuation_frame_ ;
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    frame::frame()
      //: metadata_{{'\0','\0','\0','\0','\0','\0','\0','\0','\0'}}
    {
      this->metadata_.fill('\0');
      frame_type t = frame_type::invalid_type;
      memcpy(this->metadata_.data() + 3, &t, 1);
    };
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    frame::frame(http::data_frame&& payload, std::uint32_t stream_id)
    {
      this->init_meta(frame_type::data, payload.serialized_length(), stream_id, payload.flags());
      new (&this->payload_.data_frame_) http::data_frame(std::move(payload));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    frame::frame(http::headers_frame&& payload, std::uint32_t stream_id)
    {
      this->init_meta(frame_type::headers, payload.serialized_length(), stream_id, payload.flags());
      new (&this->payload_.headers_frame_) http::headers_frame(std::move(payload));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    frame::frame(http::priority_frame&& payload, std::uint32_t stream_id)
    {
      this->init_meta(frame_type::priority, payload.serialized_length(), stream_id, payload.flags());
      new (&this->payload_.priority_frame_) http::priority_frame(std::move(payload));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    frame::frame(http::rst_stream_frame&& payload, std::uint32_t stream_id)
    {
      this->init_meta(frame_type::rst_stream, payload.serialized_length(), stream_id, payload.flags());
      new (&this->payload_.rst_stream_frame_) http::rst_stream_frame(std::move(payload));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    frame::frame(http::settings_frame&& payload, std::uint32_t stream_id)
    {
      this->init_meta(frame_type::settings, payload.serialized_length(), stream_id, payload.flags());
      new (&this->payload_.settings_frame_) http::settings_frame(std::move(payload));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    frame::frame(http::push_promise_frame&& payload, std::uint32_t stream_id)
    {
      this->init_meta(frame_type::push_promise, payload.serialized_length(), stream_id, payload.flags());
      new (&this->payload_.push_promise_frame_) http::push_promise_frame(std::move(payload));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    frame::frame(http::ping_frame&& payload, std::uint32_t stream_id)
    {
      this->init_meta(frame_type::ping, payload.serialized_length(), stream_id, payload.flags());
      new (&this->payload_.ping_frame_) http::ping_frame(std::move(payload));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    frame::frame(http::goaway_frame&& payload, std::uint32_t stream_id)
    {
      this->init_meta(frame_type::goaway, payload.serialized_length(), stream_id, payload.flags());
      new (&this->payload_.goaway_frame_) http::goaway_frame(std::move(payload));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    frame::frame(http::window_update_frame&& payload, std::uint32_t stream_id)
    {
      this->init_meta(frame_type::window_update, payload.serialized_length(), stream_id, payload.flags());
      new (&this->payload_.window_update_frame_) http::window_update_frame(std::move(payload));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    frame::frame(http::continuation_frame&& payload, std::uint32_t stream_id)
    {
      this->init_meta(frame_type::continuation, payload.serialized_length(), stream_id, payload.flags());
      new (&this->payload_.continuation_frame_) http::continuation_frame(std::move(payload));
    }
    //----------------------------------------------------------------//

    frame::frame(frame&& source)
    {
      this->metadata_.fill('\0');
      frame_type t = frame_type::invalid_type;
      memcpy(this->metadata_.data() + 3, &t, 1);

      operator=(std::move(source));
    }

    frame& frame::operator=(frame&& source)
    {
      if (&source != this)
      {
        this->destroy_union();
        frame_type t = source.type();
        this->init_meta(t, source.payload_length(), source.stream_id(), source.flags());

        switch (t)
        {
          case frame_type::data           : new(&this->payload_.data_frame_         ) http::data_frame(std::move(source.payload_.data_frame_))                  ; break;
          case frame_type::headers        : new(&this->payload_.headers_frame_      ) http::headers_frame(std::move(source.payload_.headers_frame_))            ; break;
          case frame_type::priority       : new(&this->payload_.priority_frame_     ) http::priority_frame(std::move(source.payload_.priority_frame_))          ; break;
          case frame_type::rst_stream     : new(&this->payload_.rst_stream_frame_   ) http::rst_stream_frame(std::move(source.payload_.rst_stream_frame_) )     ; break;
          case frame_type::settings       : new(&this->payload_.settings_frame_     ) http::settings_frame(std::move(source.payload_.settings_frame_) )         ; break;
          case frame_type::push_promise   : new(&this->payload_.push_promise_frame_ ) http::push_promise_frame(std::move(source.payload_.push_promise_frame_))  ; break;
          case frame_type::ping           : new(&this->payload_.ping_frame_         ) http::ping_frame(std::move(source.payload_.ping_frame_) )                 ; break;
          case frame_type::goaway         : new(&this->payload_.goaway_frame_       ) http::goaway_frame(std::move(source.payload_.goaway_frame_) )             ; break;
          case frame_type::window_update  : new(&this->payload_.window_update_frame_) http::window_update_frame(std::move(source.payload_.window_update_frame_)); break;
          case frame_type::continuation   : new(&this->payload_.continuation_frame_ ) http::continuation_frame(std::move(source.payload_.continuation_frame_))  ; break;
          case frame_type::invalid_type   : break;
        }
        this->metadata_ = std::move(source.metadata_);
      }
      return (*this);
    }
    //----------------------------------------------------------------//
    frame::~frame()
    {
      this->destroy_union();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void frame::init_meta(frame_type t, std::uint32_t payload_length, std::uint32_t stream_id, std::uint8_t flags)
    {
      std::uint32_t payload_length_24bit_nbo = htonl(payload_length << 8);
      std::uint32_t stream_id_nbo = htonl(stream_id);
      memcpy(this->metadata_.data(), &payload_length_24bit_nbo, 3);
      memcpy(this->metadata_.data() + 3, &t, 1);
      memcpy(this->metadata_.data() + 4, &flags, 1);
      memcpy(this->metadata_.data() + 5, &stream_id_nbo, 4); // assuming first bit is zero.
    };
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t frame::payload_length() const
    {
      std::uint32_t ret = 0;
      memcpy(&ret, this->metadata_.data(), 3);
      return ((ntohl(ret) >> 8) & 0x00FFFFFF);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    frame_type frame::type() const
    {
      std::uint8_t tmp;
      memcpy(&tmp, this->metadata_.data() + 3, 1);

      switch (tmp)
      {
        case (int)frame_type::data           : return frame_type::data         ;
        case (int)frame_type::headers        : return frame_type::headers      ;
        case (int)frame_type::priority       : return frame_type::priority     ;
        case (int)frame_type::rst_stream     : return frame_type::rst_stream   ;
        case (int)frame_type::settings       : return frame_type::settings     ;
        case (int)frame_type::push_promise   : return frame_type::push_promise ;
        case (int)frame_type::ping           : return frame_type::ping         ;
        case (int)frame_type::goaway         : return frame_type::goaway       ;
        case (int)frame_type::window_update  : return frame_type::window_update;
        case (int)frame_type::continuation   : return frame_type::continuation ;
        default: return frame_type::invalid_type ;
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint8_t frame::flags() const
    {
      std::uint8_t ret;
      memcpy(&ret, this->metadata_.data() + 4, 1);
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint32_t frame::stream_id() const
    {
      std::uint32_t ret;
      memcpy(&ret, this->metadata_.data() + 5, 4);
      return (0x7FFFFFFF & ntohl(ret));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    template<> bool frame::is<http::data_frame>()           const { return this->type() == frame_type::data           ; }
    template<> bool frame::is<http::headers_frame>()        const { return this->type() == frame_type::headers        ; }
    template<> bool frame::is<http::priority_frame>()       const { return this->type() == frame_type::priority       ; }
    template<> bool frame::is<http::rst_stream_frame>()     const { return this->type() == frame_type::rst_stream     ; }
    template<> bool frame::is<http::settings_frame>()       const { return this->type() == frame_type::settings       ; }
    template<> bool frame::is<http::push_promise_frame>()   const { return this->type() == frame_type::push_promise   ; }
    template<> bool frame::is<http::ping_frame>()           const { return this->type() == frame_type::ping           ; }
    template<> bool frame::is<http::goaway_frame>()         const { return this->type() == frame_type::goaway         ; }
    template<> bool frame::is<http::window_update_frame>()  const { return this->type() == frame_type::window_update  ; }
    template<> bool frame::is<http::continuation_frame>()   const { return this->type() == frame_type::continuation   ; }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    http::data_frame&           frame::data_frame()          { return this->payload_.data_frame_         ; }
    http::headers_frame&        frame::headers_frame()       { return this->payload_.headers_frame_      ; }
    http::priority_frame&       frame::priority_frame()      { return this->payload_.priority_frame_     ; }
    http::rst_stream_frame&     frame::rst_stream_frame()    { return this->payload_.rst_stream_frame_   ; }
    http::settings_frame&       frame::settings_frame()      { return this->payload_.settings_frame_     ; }
    http::push_promise_frame&   frame::push_promise_frame()  { return this->payload_.push_promise_frame_ ; }
    http::ping_frame&           frame::ping_frame()          { return this->payload_.ping_frame_         ; }
    http::goaway_frame&         frame::goaway_frame()        { return this->payload_.goaway_frame_       ; }
    http::window_update_frame&  frame::window_update_frame() { return this->payload_.window_update_frame_; }
    http::continuation_frame&   frame::continuation_frame()  { return this->payload_.continuation_frame_ ; }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const http::data_frame&           frame::data_frame()          const { return this->type() == frame_type::data           ? this->payload_.data_frame_           : frame::default_data_frame_          ; }
    const http::headers_frame&        frame::headers_frame()       const { return this->type() == frame_type::headers        ? this->payload_.headers_frame_        : frame::default_headers_frame_       ; }
    const http::priority_frame&       frame::priority_frame()      const { return this->type() == frame_type::priority       ? this->payload_.priority_frame_       : frame::default_priority_frame_      ; }
    const http::rst_stream_frame&     frame::rst_stream_frame()    const { return this->type() == frame_type::rst_stream     ? this->payload_.rst_stream_frame_     : frame::default_rst_stream_frame_    ; }
    const http::settings_frame&       frame::settings_frame()      const { return this->type() == frame_type::settings       ? this->payload_.settings_frame_       : frame::default_settings_frame_      ; }
    const http::push_promise_frame&   frame::push_promise_frame()  const { return this->type() == frame_type::push_promise   ? this->payload_.push_promise_frame_   : frame::default_push_promise_frame_  ; }
    const http::ping_frame&           frame::ping_frame()          const { return this->type() == frame_type::ping           ? this->payload_.ping_frame_           : frame::default_ping_frame_          ; }
    const http::goaway_frame&         frame::goaway_frame()        const { return this->type() == frame_type::goaway         ? this->payload_.goaway_frame_         : frame::default_goaway_frame_        ; }
    const http::window_update_frame&  frame::window_update_frame() const { return this->type() == frame_type::window_update  ? this->payload_.window_update_frame_  : frame::default_window_update_frame_ ; }
    const http::continuation_frame&   frame::continuation_frame()  const { return this->type() == frame_type::continuation   ? this->payload_.continuation_frame_   : frame::default_continuation_frame_  ; }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void frame::destroy_union()
    {
      if (this->is<http::data_frame>())
      {
        this->payload_.data_frame_.~data_frame();
      }
      else if (this->is<http::headers_frame>())
      {
        this->payload_.headers_frame_.~headers_frame();
      }
      else if (this->is<http::priority_frame>())
      {
        this->payload_.priority_frame_.~priority_frame();
      }
      else if (this->is<http::rst_stream_frame>())
      {
        this->payload_.rst_stream_frame_.~rst_stream_frame();
      }
      else if (this->is<http::settings_frame>())
      {
        this->payload_.settings_frame_.~settings_frame();
      }
      else if (this->is<http::push_promise_frame>())
      {
        this->payload_.push_promise_frame_.~push_promise_frame();
      }
      else if (this->is<http::ping_frame>())
      {
        this->payload_.ping_frame_.~ping_frame();
      }
      else if (this->is<http::goaway_frame>())
      {
        this->payload_.goaway_frame_.~goaway_frame();
      }
      else if (this->is<http::window_update_frame>())
      {
        this->payload_.window_update_frame_.~window_update_frame();
      }
      else if (this->is<http::continuation_frame>())
      {
        this->payload_.continuation_frame_.~continuation_frame();
      }
      frame_type t = frame_type::invalid_type;
      memcpy(this->metadata_.data() + 3, &t, 1);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void frame::recv_frame(socket& sock, frame& destination, const std::function<void(const std::error_code& ec)>& cb)
    {
      destination.destroy_union();
      sock.recv(destination.metadata_.data(), 9, [&sock, &destination, cb](const std::error_code& ec, std::size_t bytes_read)
      {
        if (ec)
        {
          cb ? cb(ec) : void();
        }
        else
        {
          log(destination, log_dir::incoming);
          if (destination.is<http::data_frame>())
          {
            new (&destination.payload_.data_frame_) http::data_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.data_frame_, destination.payload_length(), destination.flags(), cb);
          }
          else if (destination.is<http::headers_frame>())
          {
            new (&destination.payload_.headers_frame_) http::headers_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.headers_frame_, destination.payload_length(), destination.flags(), cb);
          }
          else if (destination.is<http::priority_frame>())
          {
            new (&destination.payload_.priority_frame_) http::priority_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.priority_frame_, destination.payload_length(), destination.flags(), cb);
          }
          else if (destination.is<http::rst_stream_frame>())
          {
            new (&destination.payload_.rst_stream_frame_) http::rst_stream_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.rst_stream_frame_, destination.payload_length(), destination.flags(), cb);
          }
          else if (destination.is<http::settings_frame>())
          {
            new (&destination.payload_.settings_frame_) http::settings_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.settings_frame_, destination.payload_length(), destination.flags(), cb);
          }
          else if (destination.is<http::push_promise_frame>())
          {
            new (&destination.payload_.push_promise_frame_) http::push_promise_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.push_promise_frame_, destination.payload_length(), destination.flags(), cb);
          }
          else if (destination.is<http::ping_frame>())
          {
            new (&destination.payload_.ping_frame_) http::ping_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.ping_frame_, destination.payload_length(), destination.flags(), cb);
          }
          else if (destination.is<http::goaway_frame>())
          {
            new (&destination.payload_.goaway_frame_) http::goaway_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.goaway_frame_, destination.payload_length(), destination.flags(), cb);
          }
          else if (destination.is<http::window_update_frame>())
          {
            new (&destination.payload_.window_update_frame_) http::window_update_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.window_update_frame_, destination.payload_length(), destination.flags(), cb);
          }
          else if (destination.is<http::continuation_frame>())
          {
            new (&destination.payload_.continuation_frame_) http::continuation_frame();
            frame_payload_base::recv_frame_payload(sock, destination.payload_.continuation_frame_, destination.payload_length(), destination.flags(), cb);
          }
          else
          {
            frame_type t = frame_type::invalid_type;
            memcpy(destination.metadata_.data() + 3, &t, 1);
            // TODO: Invalid Frame Error;
            cb ? cb(std::make_error_code(std::errc::bad_message)) : void();
          }
        }
      });
    }
    //template void frame::recv_frame<asio::ip::tcp::socket>(asio::ip::tcp::socket& sock, frame& source, const std::function<void(const std::error_code& ec)>& cb);
    //template void frame::recv_frame<asio::ssl::stream<asio::ip::tcp::socket>>(asio::ssl::stream<asio::ip::tcp::socket>& sock, frame& source, const std::function<void(const std::error_code& ec)>& cb);
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void frame::send_frame(socket& sock, const frame& source, const std::function<void(const std::error_code& ec)>& cb)
    {
      log(source, log_dir::outgoing);
      sock.send(source.metadata_.data(), source.metadata_.size(), [&sock, &source, cb](const std::error_code& ec, std::size_t bytes_transfered)
      {
        if (ec)
        {
          cb ? cb(ec) : void();
        }
        else
        {
          if (source.is<http::data_frame>()) frame_payload_base::send_frame_payload(sock, source.payload_.data_frame_, cb);
          else if (source.is<http::headers_frame>()) frame_payload_base::send_frame_payload(sock, source.payload_.headers_frame_, cb);
          else if (source.is<http::priority_frame>()) frame_payload_base::send_frame_payload(sock, source.payload_.priority_frame_, cb);
          else if (source.is<http::rst_stream_frame>()) frame_payload_base::send_frame_payload(sock, source.payload_.rst_stream_frame_, cb);
          else if (source.is<http::settings_frame>()) frame_payload_base::send_frame_payload(sock, source.payload_.settings_frame_, cb);
          else if (source.is<http::push_promise_frame>()) frame_payload_base::send_frame_payload(sock, source.payload_.push_promise_frame_, cb);
          else if (source.is<http::ping_frame>()) frame_payload_base::send_frame_payload(sock, source.payload_.ping_frame_, cb);
          else if (source.is<http::goaway_frame>()) frame_payload_base::send_frame_payload(sock, source.payload_.goaway_frame_, cb);
          else if (source.is<http::window_update_frame>()) frame_payload_base::send_frame_payload(sock, source.payload_.window_update_frame_, cb);
          else if (source.is<http::continuation_frame>()) frame_payload_base::send_frame_payload(sock, source.payload_.continuation_frame_, cb);
          else
          {
            // TODO: Invalid Frame Error;
            cb ? cb(std::make_error_code(std::errc::bad_message)) : void();
          }
        }
      });
    }
    //template void frame::send_frame<asio::ip::tcp::socket>(asio::ip::tcp::socket& sock, const frame& source, const std::function<void(const std::error_code& ec)>& cb);
    //template void frame::send_frame<asio::ssl::stream<asio::ip::tcp::socket>>(asio::ssl::stream<asio::ip::tcp::socket>& sock, const frame& source, const std::function<void(const std::error_code& ec)>& cb);
    //----------------------------------------------------------------//
    //****************************************************************//
  }
}

#endif //MANIFOLD_DISABLE_HTTP2