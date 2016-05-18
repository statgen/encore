
#include <sstream>
#include <algorithm>

#include "http_v1_request_head.hpp"

namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    static const std::string empty_string;
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v1_request_head::v1_request_head(const request_head& generic_head)
    {
      this->headers_ = generic_head.raw_headers();
      this->authority(generic_head.authority());
      this->method(generic_head.method());
      this->path(generic_head.path());
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v1_request_head::v1_request_head(const std::string& path, const std::string& method, std::list<std::pair<std::string,std::string>>&& headers)
    {
      this->headers_ = std::move(headers);
      this->path(path);
      this->method(method);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v1_request_head::v1_request_head(const std::string& path, http::method method, std::list<std::pair<std::string,std::string>>&& headers)
    {
      this->headers_ = std::move(headers);
      this->path(path);
      this->method(method);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v1_request_head::~v1_request_head()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& v1_request_head::method() const
    {
      return this->method_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_request_head::method(const std::string& value)
    {
      this->method_ = value;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_request_head::method(http::method value)
    {
      this->method(method_enum_to_string(value));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& v1_request_head::path() const
    {

      return this->path_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_request_head::path(const std::string& value)
    {
      this->path_ = value;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& v1_request_head::scheme() const
    {
      return empty_string;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_request_head::scheme(const std::string& value)
    {

    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& v1_request_head::authority() const
    {
      return this->header("host");
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_request_head::authority(const std::string& value)
    {
      this->header("host", value);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool v1_request_head::start_line(std::string&& value)
    {
      bool ret = false;

      std::size_t first_space = value.find(' ');
      std::size_t last_space = value.rfind(' ');
      if (first_space != std::string::npos && last_space != std::string::npos)
      {
        this->method_ = value.substr(0, first_space);
        ++first_space;
        this->path_ = value.substr(first_space, last_space - first_space);
        ret = true;
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::string v1_request_head::start_line() const
    {
      std::string ret(this->method_ + " " + this->path_ + " HTTP/1.1");
      return ret;
    }
    //----------------------------------------------------------------//
  }
}