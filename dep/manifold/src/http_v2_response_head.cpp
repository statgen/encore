
#include <sstream>

#include "http_v2_response_head.hpp"

namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    v2_response_head::v2_response_head()
    {
      this->status_code(status_code::ok);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_response_head::v2_response_head(const response_head& generic_head)
      : v2_header_block(generic_head)
    {
      this->status_code(generic_head.status_code());
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_response_head::v2_response_head(v2_header_block&& hb)
      : v2_header_block(std::move(hb))
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_response_head::v2_response_head(unsigned short status, std::list<hpack::header_field>&& headers)
    {
      this->headers_ = std::move(headers);
      this->status_code(status);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_response_head::v2_response_head(http::status_code status, std::list<hpack::header_field>&& headers)
    {
      this->headers_ = std::move(headers);
      this->status_code(status);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v2_response_head::~v2_response_head()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    unsigned short v2_response_head::status_code() const
    {
      std::stringstream tmp(this->header(":status"));
      unsigned short ret = 0;
      tmp >> ret;
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_response_head::status_code(unsigned short value)
    {
      this->pseudo_header(":status", std::to_string(value));
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v2_response_head::status_code(http::status_code value)
    {
      this->pseudo_header(":status", std::to_string((unsigned short)value));
    }
    //----------------------------------------------------------------//
  }
}