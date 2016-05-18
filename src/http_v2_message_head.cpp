
#include <algorithm>
#include <system_error>

#include "http_v2_message_head.hpp"

namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    static const std::string empty_string;
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_header_block::v2_header_block()
    {

    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_header_block::v2_header_block(const header_block& generic_head)
    {
      for (auto it = generic_head.raw_headers().begin(); it != generic_head.raw_headers().end(); ++it)
        this->header(it->first, it->second);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_header_block::v2_header_block(std::list<hpack::header_field>&& raw_headers)
      : headers_(std::move(raw_headers))
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_header_block::~v2_header_block()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_header_block::pseudo_header(const std::string& name, const std::string& value)
    {
      std::string n(name);
      std::string v(value);
      this->pseudo_header(std::move(n), std::move(v));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_header_block::pseudo_header(std::string&& name, std::string&& value)
    {
      const std::string whitespace(" \t\f\v\r\n");
      value.erase(0, value.find_first_not_of(whitespace));
      value.erase(value.find_last_not_of(whitespace)+1);

      auto first_non_pseudo_header_itr = this->headers_.begin();
      for ( ; first_non_pseudo_header_itr != this->headers_.end(); ++first_non_pseudo_header_itr)
      {
        if (first_non_pseudo_header_itr->name.size() && first_non_pseudo_header_itr->name.front() != ':')
          break;
      }

      for (auto it = this->headers_.begin(); it != first_non_pseudo_header_itr;)
      {
        if (it->name == name)
        {
          this->headers_.erase(it);
          it = first_non_pseudo_header_itr; // Breaking here because multiple psuedo headers prevented.
        }
        else
          ++it;
      }

      this->headers_.insert(first_non_pseudo_header_itr, hpack::header_field(std::move(name), std::move(value)));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool v2_header_block::header_exists(const std::string& name) const
    {
      return this->header_exists(std::string(name));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool v2_header_block::header_exists(std::string&& name) const
    {
      bool ret = false;
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);
      for (auto it = this->headers_.begin(); !ret && it != this->headers_.end(); ++it)
      {
        if (it->name == name)
          ret = true;
      }
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_header_block::header(const std::string& name, const std::string& value)
    {
      std::string n(name);
      std::string v(value);
      this->header(std::move(n), std::move(v));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_header_block::header(std::string&& name, std::string&& value)
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
        if (it->name == name)
          it = this->headers_.erase(it);
        else
          ++it;
      }
      this->headers_.push_back(hpack::header_field(std::move(name), std::move(value)));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_header_block::multi_header(const std::string& name, const std::list<std::string>& values)
    {
      std::string n(name);
      std::list<std::string> v(values);
      this->multi_header(std::move(n), std::move(v));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_header_block::multi_header(std::string&& name, std::list<std::string>&& values)
    {
      // trim
      const std::string whitespace(" \t\f\v\r\n");
      name.erase(0, name.find_first_not_of(":" + whitespace));
      name.erase(name.find_last_not_of(whitespace)+1);

      // make name lowercase
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);

      for (auto it = this->headers_.begin(); it != this->headers_.end();)
      {
        if (it->name == name)
          it = this->headers_.erase(it);
        else
          ++it;
      }

      std::for_each(values.begin(), values.end(), [this, &whitespace, &name](std::string& value)
      {
        value.erase(0, value.find_first_not_of(whitespace));
        value.erase(value.find_last_not_of(whitespace)+1);

        this->headers_.push_back(hpack::header_field(std::move(name), std::move(value)));
      });

    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& v2_header_block::header(const std::string& name) const
    {
      return this->header(std::string(name));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& v2_header_block::header(std::string&& name) const
    {
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);

      for (auto it = this->headers_.rbegin(); it != this->headers_.rend(); ++it)
      {
        if (it->name == name)
          return  it->value;
      }

      return empty_string;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::list<std::string> v2_header_block::multi_header(const std::string& name) const
    {
      return this->multi_header(std::string(name));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::list<std::string> v2_header_block::multi_header(std::string&& name) const
    {
      std::list<std::string> ret;
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);

      for (auto it = this->headers_.begin(); it != this->headers_.end(); ++it)
      {
        if (it->name == name)
          ret.push_back(it->value);
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::list<hpack::header_field>& v2_header_block::raw_headers() const
    {
      return this->headers_;
    }
    //----------------------------------------------------------------//

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

    //----------------------------------------------------------------//
    void v2_header_block::serialize(hpack::encoder& enc, const v2_header_block& source, std::string& destination)
    {
      enc.encode(source.headers_, destination);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool v2_header_block::deserialize(hpack::decoder& dec, const std::string& source, v2_header_block& destination)
    {
      return dec.decode(source.begin(), source.end(), destination.headers_);
    }
    //----------------------------------------------------------------//
  }
}