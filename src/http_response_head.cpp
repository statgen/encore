
#include "http_response_head.hpp"
#include "http_v1_response_head.hpp"
#include "http_v2_response_head.hpp"

namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    std::string status_code_to_reason_phrase(unsigned short status_code)
    {
      std::string ret;
      switch(status_code)
      {
        case (unsigned short)status_code::continue_status:
          ret = "Continue";
          break;
        case (unsigned short)status_code::switching_protocols:
          ret = "Switching Protocols";
          break;
        case (unsigned short)status_code::ok:
          ret = "OK";
          break;
        case (unsigned short)status_code::created:
          ret = "Created";
          break;
        case (unsigned short)status_code::accepted:
          ret = "Accepted";
          break;
        case (unsigned short)status_code::non_authoritative_information:
          ret = "Non-Authoritative Information";
          break;
        case (unsigned short)status_code::no_content:
          ret = "No Content";
          break;
        case (unsigned short)status_code::reset_content:
          ret = "Reset Content";
          break;
        case (unsigned short)status_code::partial_content:
          ret = "Partial Content";
          break;
        case (unsigned short)status_code::multiple_choices:
          ret = "Multiple Choices";
          break;
        case (unsigned short)status_code::moved_permanently:
          ret = "Moved Permanently";
          break;
        case (unsigned short)status_code::found:
          ret = "Found";
          break;
        case (unsigned short)status_code::see_other:
          ret = "See Other";
          break;
        case (unsigned short)status_code::not_modified:
          ret = "Not Modified";
          break;
        case (unsigned short)status_code::use_proxy:
          ret = "Use Proxy";
          break;
        case (unsigned short)status_code::temporary_redirect:
          ret = "Temporary Redirect";
          break;
        case (unsigned short)status_code::bad_request:
          ret = "Bad Request";
          break;
        case (unsigned short)status_code::unauthorized:
          ret = "Unauthorized";
          break;
        case (unsigned short)status_code::payment_required:
          ret = "Payment Required";
          break;
        case (unsigned short)status_code::forbidden:
          ret = "Forbidden";
          break;
        case (unsigned short)status_code::not_found:
          ret = "Not Found";
          break;
        case (unsigned short)status_code::method_not_allowed:
          ret = "Method Not Allowed";
          break;
        case (unsigned short)status_code::not_acceptable:
          ret = "Not Acceptable";
          break;
        case (unsigned short)status_code::proxy_authentication_required:
          ret = "Proxy Authentication Required";
          break;
        case (unsigned short)status_code::request_timeout:
          ret = "Request Timeout";
          break;
        case (unsigned short)status_code::conflict:
          ret = "Conflict";
          break;
        case (unsigned short)status_code::gone:
          ret = "Gone";
          break;
        case (unsigned short)status_code::length_required:
          ret = "Length Required";
          break;
        case (unsigned short)status_code::precondition_failed:
          ret = "Precondition Failed";
          break;
        case (unsigned short)status_code::request_entity_too_large:
          ret = "Request Entity Too Large";
          break;
        case (unsigned short)status_code::request_uri_too_long:
          ret = "Request-URI Too Long";
          break;
        case (unsigned short)status_code::unsupported_media_type:
          ret = "Unsupported Media Type";
          break;
        case (unsigned short)status_code::requested_range_not_satisfiable:
          ret = "Requested Range Not Satisfiable";
          break;
        case (unsigned short)status_code::expectation_failed:
          ret = "Expectation Failed";
          break;
        case (unsigned short)status_code::internal_server_error:
          ret = "Internal Server Error";
          break;
        case (unsigned short)status_code::not_implemented:
          ret = "Not Implemented";
          break;
        case (unsigned short)status_code::bad_gateway:
          ret = "Bad Gateway";
          break;
        case (unsigned short)status_code::service_unavailable:
          ret = "Service Unavailable";
          break;
        case (unsigned short)status_code::gateway_timeout:
          ret = "Gateway Timeout";
          break;
        case (unsigned short)status_code::http_version_not_supported:
          ret = "HTTP Version Not Supported";
          break;
        default:
          ret = "Unknown Reason Phrase";
          break;
      }

      return ret;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    response_head::response_head(std::uint16_t status, std::list<std::pair<std::string, std::string>>&& headers) : header_block(std::move(headers)), status_code_(status)
    {
    }
    //----------------------------------------------------------------//

//    //----------------------------------------------------------------//
//    response_head::response_head(v1_message_head&& v1_headers)
//      : response_head(v1_response_head(std::move(v1_headers)))
//    {
//    }
//    //----------------------------------------------------------------//

//    //----------------------------------------------------------------//
//    response_head::response_head(v2_header_block&& v2_headers)
//      : response_head(v2_response_head(std::move(v2_headers)))
//    {
//    }
//    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    response_head::response_head(const v1_response_head& v1_headers)
      : header_block(v1_headers)
    {
      this->status_code(v1_headers.status_code());
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    response_head::response_head(const v2_response_head& v2_headers)
      : header_block(v2_headers)
    {
      this->status_code(v2_headers.status_code());
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    response_head::~response_head()
    {
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    std::uint16_t response_head::status_code() const
    {
      return this->status_code_;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void response_head::status_code(std::uint16_t value)
    {
      this->status_code_ = value;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void response_head::status_code(http::status_code value)
    {
      this->status_code_ = (std::uint16_t)value;
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool response_head::has_informational_status() const
    {
      std::uint16_t status = this->status_code();
      return (status >= 100 && status < 200);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool response_head::has_successful_status() const
    {
      std::uint16_t status = this->status_code();
      return (status >= 200 && status < 300);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool response_head::has_redirection_status() const
    {
      std::uint16_t status = this->status_code();
      return (status >= 300 && status < 400);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool response_head::has_client_error_status() const
    {
      std::uint16_t status = this->status_code();
      return (status >= 400 && status < 500);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    bool response_head::has_server_error_status() const
    {
      std::uint16_t status = this->status_code();
      return (status >= 500 && status < 600);
    }
    //----------------------------------------------------------------//
  }
}
