//
// Created by Jonathon LeFaive on 1/6/16.
//

#include "http_message_head.hpp"

#include <algorithm>
#include <system_error>

#include "http_v1_message_head.hpp"
#include "http_v2_message_head.hpp"

namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    static const std::string empty_string;
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    header_block::header_block()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    header_block::header_block(const v1_header_block& v1_headers)
    {
      this->headers_ = v1_headers.raw_headers();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    header_block::header_block(const v2_header_block& v2_headers)
    {
      for (auto it = v2_headers.raw_headers().begin(); it != v2_headers.raw_headers().end(); ++it)
      {
        if (it->name.front() != ':')
          this->headers_.emplace_back(it->name, it->value);
      }
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    header_block::header_block(std::list<std::pair<std::string, std::string>>&& headers)
      : headers_(std::move(headers))
    {

    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    header_block::~header_block()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool header_block::header_exists(const std::string& name) const
    {
      return this->header_exists(std::string(name));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool header_block::header_exists(std::string&& name) const
    {
      bool ret = false;
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);
      for (auto it = this->headers_.begin(); !ret && it != this->headers_.end(); ++it)
      {
        if (it->first == name)
          ret = true;
      }
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void header_block::header(const std::string& name, const std::string& value)
    {
      std::string n(name);
      std::string v(value);
      this->header(std::move(n), std::move(v));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void header_block::header(std::string&& name, std::string&& value)
    {
      // trim
      const std::string whitespace(" \t\f\v\r\n");
      name.erase(0, name.find_first_not_of(":" + whitespace));
      name.erase(name.find_last_not_of(whitespace)+1);
      value.erase(0, value.find_first_not_of(whitespace));
      value.erase(value.find_last_not_of(whitespace)+1);

      // make name lowercase
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);

      for (auto it = this->headers_.begin(); it != this->headers_.end();)
      {
        if (it->first == name)
          it = this->headers_.erase(it);
        else
          ++it;
      }
      this->headers_.push_back(std::pair<std::string,std::string>(std::move(name), std::move(value)));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void header_block::multi_header(const std::string& name, const std::list<std::string>& values)
    {
      std::string n(name);
      std::list<std::string> v(values);
      this->multi_header(std::move(n), std::move(v));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void header_block::multi_header(std::string&& name, std::list<std::string>&& values)
    {
      // trim
      const std::string whitespace(" \t\f\v\r\n");
      name.erase(0, name.find_first_not_of(":" + whitespace));
      name.erase(name.find_last_not_of(whitespace)+1);

      // make name lowercase
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);

      for (auto it = this->headers_.begin(); it != this->headers_.end();)
      {
        if (it->first == name)
          it = this->headers_.erase(it);
        else
          ++it;
      }

      std::for_each(values.begin(), values.end(), [this, &whitespace, &name](std::string& value)
      {
        value.erase(0, value.find_first_not_of(whitespace));
        value.erase(value.find_last_not_of(whitespace)+1);

        this->headers_.push_back(std::pair<std::string,std::string>(std::move(name), std::move(value)));
      });

    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& header_block::header(const std::string& name) const
    {
      return this->header(std::string(name));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& header_block::header(std::string&& name) const
    {
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);

      for (auto it = this->headers_.rbegin(); it != this->headers_.rend(); ++it)
      {
        if (it->first == name)
          return it->second;
      }

      return empty_string;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::list<std::string> header_block::multi_header(const std::string& name) const
    {
      std::list<std::string> ret;
      std::string nameToLower(name);
      std::transform(nameToLower.begin(), nameToLower.end(), nameToLower.begin(), ::tolower);

      for (auto it = this->headers_.begin(); it != this->headers_.end(); ++it)
      {
        if (it->first == nameToLower)
          ret.push_back(it->second);
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::list<std::pair<std::string,std::string>>& header_block::raw_headers() const
    {
      return this->headers_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::size_t header_block::size() const
    {
      return this->headers_.size();
    }

//    //----------------------------------------------------------------//
//    const std::string& header_block::http_version() const
//    {
//      return this->version_;
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    void  header_block::http_version(const std::string& version)
//    {
//      this->version_ = version;
//    }
//    //----------------------------------------------------------------//
  }
}