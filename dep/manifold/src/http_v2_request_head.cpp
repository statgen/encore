
#include <sstream>
#include <algorithm>

#include "http_v2_request_head.hpp"

namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    v2_request_head::v2_request_head(v2_header_block&& hb)
      : v2_header_block(std::move(hb))
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_request_head::v2_request_head(const request_head& generic_head)
      : v2_header_block(generic_head)
    {
      this->path(generic_head.path());
      this->method(generic_head.method());
      this->authority(generic_head.authority());
      this->scheme(generic_head.scheme());
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_request_head::v2_request_head(const std::string& path, const std::string& method, std::list<hpack::header_field>&& headers)
    {
      this->headers_ = std::move(headers);
      this->path(path);
      this->method(method);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_request_head::v2_request_head(const std::string& path, http::method method, std::list<hpack::header_field>&& headers)
    {
      this->headers_ = std::move(headers);
      this->path(path);
      this->method(method);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_request_head::~v2_request_head()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& v2_request_head::method() const
    {
      return this->header(":method");
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_request_head::method(const std::string& value)
    {
      std::string tmp(value);
      std::transform(tmp.begin(), tmp.end(), tmp.begin(), ::toupper);
      this->pseudo_header(":method", std::move(tmp));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_request_head::method(http::method value)
    {
      this->pseudo_header(":method", std::move(method_enum_to_string(value)));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& v2_request_head::path() const
    {
      return this->header(":path");
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_request_head::path(const std::string& value)
    {
      this->pseudo_header(":path", value);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& v2_request_head::scheme() const
    {
      return this->header(":scheme");
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_request_head::scheme(const std::string& value)
    {
      this->pseudo_header(":scheme", value);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    const std::string& v2_request_head::authority() const
    {
      return this->header(":authority");
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_request_head::authority(const std::string& value)
    {
      this->pseudo_header(":authority", value);
    }
    //----------------------------------------------------------------//
  }
}