
#include "uniform_resource_identifier.hpp"

#include <sstream>
#include <regex>
#include <assert.h>

namespace manifold
{
  std::string encode_uri(const std::string& data)
  {
    //; , / ? : @ & = + $
    std::stringstream ss;
    for(unsigned long i = 0UL; i < data.size(); i++)
    {
      int dec = (int)static_cast<unsigned char>(data[i]);
      if ( (48 <= dec && dec <= 57) || //0-9
           (65 <= dec && dec <= 90) || //ABC...XYZ
           (97 <= dec && dec <= 122) || //abc...xyz
           (data[i]=='_' || data[i]=='-' || data[i]=='.' || data[i]=='~' || data[i]=='!' || data[i]=='*' || data[i]=='(' || data[i]==')' || data[i]=='\'') || // Unescaped characters
           (data[i]==';' || data[i]==',' || data[i]=='/' || data[i]=='?' || data[i]==':' || data[i]=='@' || data[i]=='&' || data[i]=='=' || data[i]=='+' || data[i]=='$' || data[i]=='#' || data[i]=='[' || data[i]==']') // Reserved Characters
        )
      {
        ss << data[i];
      }
      else
      {
        ss << "%" << std::hex << dec;
      }
    }
    return ss.str();
  }

  std::string encode_uri_component(const std::string& data)
  {
    std::stringstream ss;

    for(unsigned long i = 0UL; i < data.size(); i++)
    {
      int dec = (int)static_cast<unsigned char>(data[i]);
      if ( (48 <= dec && dec <= 57) || //0-9
           (65 <= dec && dec <= 90) || //ABC...XYZ
           (97 <= dec && dec <= 122) || //abc...xyz
           (data[i]=='_' || data[i]=='-' || data[i]=='.' || data[i]=='~' || data[i]=='!' || data[i]=='*' || data[i]=='(' || data[i]==')' || data[i]=='\'') // Unescaped characters
        )
      {
        ss << data[i];
      }
      else
      {
        ss << "%" << std::hex << dec;
      }
    }
    return ss.str();
  }

  std::string percent_decode(const std::string& encoded_data)
  {
    std::stringstream ss;
    size_t i = 0;
    while(i < encoded_data.length())
    {
      if (encoded_data[i] == '+')
      {
        ss << ' ';
        ++i;
      }
      else if (encoded_data[i] == '%' && encoded_data.size() > (i + 2))
      {
        ++i;
        ss << static_cast<char>(strtoul(encoded_data.substr(i,2).c_str(), nullptr, 16));
        i = i + 2;
      }
      else
      {
        ss << encoded_data[i];
        ++i;
      }
    }
    return ss.str();
  }

  void uri::parse_authority(const std::string& authority)
  {
    std::string socket_address;
    size_t at_pos = authority.find("@");
    if (at_pos != std::string::npos)
    {
      std::string user_info = authority.substr(0, at_pos);

      std::size_t colon_pos = user_info.find(":");
      if (colon_pos == std::string::npos)
      {
        this->username_ = user_info;
      }
      else
      {
        this->username_ = user_info.substr(0, colon_pos);
        if (user_info.length() > (colon_pos + 1))
          this->password_ = user_info.substr(colon_pos + 1);
      }

      if (authority.size() > at_pos + 1)
        socket_address = authority.substr(at_pos + 1);
    }
    else
    {
      socket_address = authority;
    }

    std::smatch r;
    std::regex exp("(.+):([0-9]+)$");
    std::regex_match(socket_address, r, exp);
    if (r.size() == 3)
    {
      this->host_ = r[1].str();
      this->port_ = static_cast<unsigned short>(atol(r[2].str().c_str()));
    }
    else
    {
      this->host_ = socket_address;
    }
  }

  /*
  IETF RegExp https://tools.ietf.org/html/rfc3986#page-50
  "^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?"

  scheme    = $2
  authority = $4
  path      = $5
  query     = $7
  fragment  = $9

  $1 = http:
  $2 = http
  $3 = //www.ics.uci.edu
  $4 = www.ics.uci.edu
  $5 = /pub/ietf/uri/
  $6 = <undefined>
  $7 = <undefined>
  $8 = #Related
  $9 = Related
  */
  void uri::parse_uri(const std::string& string_uri)
  {
    std::smatch r;
    std::regex exp("^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\\?([^#]*))?(#(.*))?"); // IETF
    std::regex_match(string_uri, r, exp);
    if (!r.empty())
    {
      assert(r.size() == 10);

      // absolute path
      if (r[2].matched)
      {
        this->scheme_name_ = r[2].str();
        std::transform(this->scheme_name_.begin(), this->scheme_name_.end(), this->scheme_name_.begin(), ::tolower);
      }
      if (r[4].matched)
      {
        this->parse_authority(r[4].str());
      }

      if (r[5].matched)
      {
        this->path_ = r[5].str();
      }

      if (r[7].matched)
      {
        this->query_ = r[7].str();
      }

      if (r[9].matched)
      {
        this->fragment_ = r[9].str();
      }
    }
  }

  std::string uri::basename() const
  {
    std::string ret;
    const size_t last_slash = this->path_.rfind("/");
    if (last_slash != std::string::npos && last_slash != this->path_.length())
    {
      ret = this->path_.substr(last_slash + 1);
    }
    return ret;
  }

  std::string uri::socket_address() const
  {
    std::stringstream ss;
    ss << this->host_;
    ss << ":";
    ss << this->port_;
    return ss.str();
  }

  std::string uri::authority() const
  {
    std::stringstream ss;

    if (!this->username_.empty() || !this->password_.empty())
    {
      ss << this->username_;
      if (!this->password_.empty())
        ss << ":" << this->password_;
      ss << "@";
    }

    ss << this->host_;
    if (this->port_)
    {
      ss << ":";
      ss << this->port_;
    }

    return ss.str();
  }

  std::string uri::to_string() const
  {
    std::stringstream ss;
    if (!this->scheme_name_.empty())
      ss<< this->scheme_name_ << "://";
    ss << this->authority();
    ss << this->path_with_query();
    if (!this->fragment_.empty())
      ss << "#" << this->fragment_;
    return ss.str();
  }

  std::string uri::path_with_query() const
  {
    std::stringstream ss;
    ss << this->path_;
    if (!this->query_.empty())
      ss << "?" << this->query_;
    return ss.str();
  }
}