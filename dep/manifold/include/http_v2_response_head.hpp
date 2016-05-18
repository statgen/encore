#pragma once

#ifndef MANIFOLD_HTTP_V2_RESPONSE_HEAD_HPP
#define MANIFOLD_HTTP_V2_RESPONSE_HEAD_HPP

#include "http_v2_message_head.hpp"
#include "http_response_head.hpp"

namespace manifold
{
  namespace http
  {
    //================================================================//
    class v2_response_head : public v2_header_block
    {
    private:
    public:
      //----------------------------------------------------------------//
      v2_response_head();
      v2_response_head(const response_head& generic_head);
      v2_response_head(v2_header_block&& hb);
      v2_response_head(unsigned short status, std::list<hpack::header_field>&& headers = {});
      v2_response_head(http::status_code status, std::list<hpack::header_field>&& headers = {});
      ~v2_response_head();
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      unsigned short status_code() const;
      void status_code(unsigned short value);
      void status_code(http::status_code value);
      //----------------------------------------------------------------//
    };
    //================================================================//
  };
}

#endif // MANIFOLD_HTTP_V2_RESPONSE_HEAD_HPP