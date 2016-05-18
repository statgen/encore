#pragma once

#ifndef MANIFOLD_HTTP_RESPONSE_HEAD_HPP
#define MANIFOLD_HTTP_RESPONSE_HEAD_HPP

#include "http_message_head.hpp"

namespace manifold
{
  namespace http
  {
    //================================================================//
    enum class status_code : std::uint16_t
    {
      // informational
      continue_status                 = 100,
      switching_protocols             = 101,

      // successful
      ok                              = 200,
      created                         = 201,
      accepted                        = 202,
      non_authoritative_information   = 203,
      no_content                      = 204,
      reset_content                   = 205,
      partial_content                 = 206,

      // redirection
      multiple_choices                = 300,
      moved_permanently               = 301,
      found                           = 302,
      see_other                       = 303,
      not_modified                    = 304,
      use_proxy                       = 305,
      // 306 unused
      temporary_redirect              = 307,

      // client error
      bad_request                     = 400,
      unauthorized                    = 401,
      payment_required                = 402,
      forbidden                       = 403,
      not_found                       = 404,
      method_not_allowed              = 405,
      not_acceptable                  = 406,
      proxy_authentication_required   = 407,
      request_timeout                 = 408,
      conflict                        = 409,
      gone                            = 410,
      length_required                 = 411,
      precondition_failed             = 412,
      request_entity_too_large         = 413,
      request_uri_too_long            = 414,
      unsupported_media_type          = 415,
      requested_range_not_satisfiable = 416,
      expectation_failed              = 417,

      // server error
      internal_server_error           = 500,
      not_implemented                 = 501,
      bad_gateway                     = 502,
      service_unavailable             = 503,
      gateway_timeout                 = 504,
      http_version_not_supported      = 505
    };
    //================================================================//

    std::string status_code_to_reason_phrase(unsigned short status_code);

    //================================================================//
    class response_head : public header_block
    {
    public:
      //----------------------------------------------------------------//
      response_head(std::uint16_t status = 200, std::list<std::pair<std::string, std::string>>&& headers = {});
      //response_head(class v1_message_head&& v1_headers);
      //response_head(class v2_header_block&& v2_headers);
      response_head(const class v1_response_head& v1_headers);
      response_head(const class v2_response_head& v2_headers);
      ~response_head();
      std::uint16_t status_code() const;
      void status_code(std::uint16_t value);
      void status_code(http::status_code value);
      bool has_informational_status() const;
      bool has_successful_status() const;
      bool has_redirection_status() const;
      bool has_client_error_status() const;
      bool has_server_error_status() const;
      //----------------------------------------------------------------//
    private:
      //----------------------------------------------------------------//
      std::uint16_t status_code_;
      //----------------------------------------------------------------//
    };
    //================================================================//
  }
}

#endif //MANIFOLD_HTTP_RESPONSE_HEAD_HPP
