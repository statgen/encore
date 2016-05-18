
#include "http_request_head.hpp"
#include "http_v1_request_head.hpp"
#include "http_v2_request_head.hpp"

namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    static const std::string base64_chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789+/";
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    static inline bool is_base64(unsigned char const c)
    {
      return (isalnum(c) || (c == '+') || (c == '/'));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::string base64_encode(unsigned char const* bytes_to_encode, size_t in_len)
    {
      std::string ret;
      size_t i = 0;

      unsigned char char_array_3[3];
      memset(char_array_3, 0, 3);
      unsigned char char_array_4[4];
      memset(char_array_4, 0, 4);

      while (in_len--)
      {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3)
        {
          char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
          char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
          char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
          char_array_4[3] = char_array_3[2] & 0x3f;

          for(i = 0; (i <4) ; i++)
            ret += base64_chars[char_array_4[i]];
          i = 0;
        }
      }

      if (i)
      {
        for(size_t j = i; j < 3; j++)
          char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (size_t j = 0; (j < i + 1); j++)
          ret += base64_chars[char_array_4[j]];

        while((i++ < 3))
          ret += '=';

      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::string base64_encode(const std::string& data)
    {
      return base64_encode(reinterpret_cast<unsigned char const*>(data.data()), data.size());
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::string base64_decode(std::string const& encoded_string) {
      size_t in_len = encoded_string.size();
      size_t i = 0;
      int in_ = 0;
      unsigned char char_array_4[4], char_array_3[3];
      memset(char_array_3, 0, 3);
      memset(char_array_4, 0, 4);
      std::string ret;

      while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
      {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i ==4) {
          for (i = 0; i <4; i++)
            char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));

          char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
          char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
          char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

          for (i = 0; (i < 3); i++)
            ret += char_array_3[i];
          i = 0;
        }
      }

      if (i)
      {
        for (size_t j = i; j <4; j++)
          char_array_4[j] = 0;

        for (size_t j = 0; j <4; j++)
          char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (size_t j = 0; (j < i - 1); j++) ret += char_array_3[j];
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::string basic_auth(const std::string& username, const std::string& password)
    {
      return ("Basic " + base64_encode(username + ":" + password));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::string method_enum_to_string(method method)
    {
      std::string ret;

      if (method == method::head)         ret = "HEAD";
      else if (method == method::get)     ret = "GET";
      else if (method == method::post)    ret = "POST";
      else if (method == method::put)     ret = "PUT";
      else if (method == method::del)     ret = "DELETE";
      else if (method == method::options) ret = "OPTIONS";
      else if (method == method::trace)   ret = "TRACE";
      else if (method == method::connect) ret = "CONNECT";
      else if (method == method::patch)   ret = "PATCH";

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool request_head::method_is(http::method methodToCheck) const
    {
      return (this->method() == method_enum_to_string(methodToCheck));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    request_head::request_head(const std::string& path, const std::string& meth, std::list<std::pair<std::string, std::string>>&& headers)
      : header_block(std::move(headers))
    {
      this->method(meth);
      this->path(path);
    }
    //----------------------------------------------------------------//

//    //----------------------------------------------------------------//
//    request_head::request_head(v1_message_head&& v1_headers)
//      : request_head(v1_request_head(std::move(v1_headers)))
//    {
//    }
//    //----------------------------------------------------------------//

//    //----------------------------------------------------------------//
//    request_head::request_head(v2_header_block&& v2_headers)
//      : request_head(v2_request_head(std::move(v2_headers)))
//    {
//    }
//    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    request_head::request_head(const v1_request_head& v1_headers)
      : header_block(v1_headers)
    {
      this->method(v1_headers.method());
      this->path(v1_headers.path());
      this->authority(v1_headers.header("host"));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    request_head::request_head(const v2_request_head& v2_headers)
      : header_block(v2_headers)
    {
      this->method(v2_headers.method());
      this->path(v2_headers.path());
      this->authority(v2_headers.authority());
      this->scheme(v2_headers.scheme());
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    request_head::request_head(const std::string& path, http::method meth, std::list<std::pair<std::string, std::string>>&& headers)
      : header_block(std::move(headers))
    {
      this->method(meth);
      this->path(path);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    request_head::~request_head()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& request_head::method() const
    {
      return this->method_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void request_head::method(const std::string& value)
    {
      this->method(std::string(value));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void request_head::method(std::string&& value)
    {
      const std::string whitespace(" \t\f\v\r\n");
      value.erase(0, value.find_first_not_of(whitespace));
      value.erase(value.find_last_not_of(whitespace) + 1);

      std::transform(value.begin(), value.end(), value.begin(), ::toupper);

      if (value.size())
        this->method_ = value;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void request_head::method(http::method value)
    {
      this->method(method_enum_to_string(value));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& request_head::path() const
    {
      return this->path_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void request_head::path(const std::string& value)
    {
      this->path_ = value.size() ? value : "/";
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& request_head::scheme() const
    {
      return this->scheme_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void request_head::scheme(const std::string& value)
    {
      this->scheme_ = value;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& request_head::authority() const
    {
      return this->authority_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void request_head::authority(const std::string& value)
    {
      this->authority_ = value;
    }
    //----------------------------------------------------------------//
  }
}