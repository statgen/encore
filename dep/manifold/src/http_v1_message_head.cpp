//
// Created by Jonathon LeFaive on 1/3/16.
//


#include <algorithm>
#include <system_error>

#include "http_v1_message_head.hpp"

namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    static const std::string empty_string;
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v1_header_block::v1_header_block()
    {

    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v1_header_block::v1_header_block(const header_block& generic_head)
    {
      this->headers_ = generic_head.raw_headers();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v1_header_block::~v1_header_block()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_header_block::header(const std::string& name, const std::string& value)
    {
      std::string n(name);
      std::string v(value);
      this->header(std::move(n), std::move(v));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_header_block::header(std::string&& name, std::string&& value)
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
      if (name.size())
        this->headers_.push_back(std::pair<std::string,std::string>(std::move(name), std::move(value)));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_header_block::multi_header(const std::string& name, const std::list<std::string>& values)
    {
      std::string n(name);
      std::list<std::string> v(values);
      this->multi_header(std::move(n), std::move(v));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_header_block::multi_header(std::string&& name, std::list<std::string>&& values)
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

      if (name.size())
      {
        std::for_each(values.begin(), values.end(), [this, &whitespace, &name](std::string &value)
        {
          value.erase(0, value.find_first_not_of(whitespace));
          value.erase(value.find_last_not_of(whitespace) + 1);
          this->headers_.push_back(std::pair<std::string, std::string>(std::move(name), std::move(value)));
        });
      }

    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool v1_header_block::header_exists(const std::string& name) const
    {
      return this->header_exists(std::string(name));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool v1_header_block::header_exists(std::string&& name) const
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
    void v1_header_block::remove_header(const std::string& name)
    {
      this->remove_header(std::string(name));
    }

    //----------------------------------------------------------------//
    void v1_header_block::remove_header(std::string&& name)
    {
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
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& v1_header_block::header(const std::string& name) const
    {
      std::string nameToLower(name);
      std::transform(nameToLower.begin(), nameToLower.end(), nameToLower.begin(), ::tolower);

      for (auto it = this->headers_.rbegin(); it != this->headers_.rend(); ++it)
      {
        if (it->first == nameToLower)
          return it->second;
      }

      return empty_string;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::list<std::string> v1_header_block::multi_header(const std::string& name) const
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
    const std::list<std::pair<std::string,std::string>>& v1_header_block::raw_headers() const
    {
      return this->headers_;
    }
    //----------------------------------------------------------------//

//    //----------------------------------------------------------------//
//    const std::string& v1_header_block::http_version() const
//    {
//      return this->version_;
//    }
//    //----------------------------------------------------------------//
//
//    //----------------------------------------------------------------//
//    void  v1_header_block::http_version(const std::string& version)
//    {
//      this->version_ = version;
//    }
//    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_header_block::serialize(const v1_header_block& source, std::ostream& destination)
    {
      for (auto it = source.headers_.begin(); it != source.headers_.end(); ++it)
      {
        destination << it->first << ": " << it->second << "\r\n";
      }

      destination << "\r\n";
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool v1_header_block::deserialize(std::istream& source, v1_header_block& destination)
    {
      while (source.good())
      {
        std::string header_line;
        std::getline(source, header_line);
        header_line.erase(header_line.find_last_not_of("\r\n") + 1);

        if (header_line.empty())
          break;

        std::size_t colon_it = header_line.find(":");
        if (colon_it == std::string::npos)
          break;

        std::string name = header_line.substr(0, colon_it);
        name.erase(0, name.find_first_not_of(" "));
        name.erase(name.find_last_not_of(" ") + 1);

        std::string value = header_line.substr(++colon_it);
        value.erase(0, value.find_first_not_of(" "));
        value.erase(value.find_last_not_of(" ") + 1);
        destination.header(std::move(name), std::move(value));
      }

      return true;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_message_head::serialize(const v1_message_head& source, std::ostream& destination)
    {
      destination << source.start_line() << "\r\n";

      v1_header_block::serialize(source, destination);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool v1_message_head::deserialize(std::istream& source, v1_message_head& destination)
    {
      bool ret = false;
      std::string start_line;
      std::getline(source, start_line);
      start_line.erase(start_line.find_last_not_of(" \r")+1);
      if (destination.start_line(std::move(start_line)))
        ret = v1_header_block::deserialize(source, destination);
      return ret;
    }
    //----------------------------------------------------------------//
  }
}