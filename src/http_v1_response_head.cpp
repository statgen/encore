
#include <sstream>

#include "http_v1_response_head.hpp"

namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    v1_response_head::v1_response_head()
    {
      this->status_code_ = (unsigned short)http::status_code::ok;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool v1_response_head::start_line(std::string&& value)
    {
      this->status_code_ = 0;
      if (value.size() >= 12)
      {
        std::stringstream tmp(value.substr(9,3));
        tmp >> this->status_code_;
      }
      return (this->status_code_ != 0);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::string v1_response_head::start_line() const
    {
      std::string ret("HTTP/1.1 " + std::to_string(this->status_code_) + " " + status_code_to_reason_phrase(this->status_code_));
      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v1_response_head::v1_response_head(const response_head& generic_head)
    {
      this->status_code_ = generic_head.status_code();
      this->headers_ = generic_head.raw_headers();
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v1_response_head::v1_response_head(unsigned short status, std::list<std::pair<std::string,std::string>>&& headers)
    {
      this->status_code_ = status;
      this->headers_ = std::move(headers);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v1_response_head::v1_response_head(http::status_code status, std::list<std::pair<std::string,std::string>>&& headers)
    {
      this->status_code(status);
      this->headers_ = std::move(headers);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    v1_response_head::~v1_response_head()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    unsigned short v1_response_head::status_code() const
    {
      return this->status_code_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_response_head::status_code(unsigned short value)
    {
      this->status_code_ = value;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void v1_response_head::status_code(http::status_code value)
    {
      this->status_code((unsigned short)value);
    }
    //----------------------------------------------------------------//
  }
}