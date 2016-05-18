#pragma once

#ifndef MANIFOLD_HTTP_V2_REQUEST_HEAD_HPP
#define MANIFOLD_HTTP_V2_REQUEST_HEAD_HPP

#include "http_v2_message_head.hpp"
#include "http_request_head.hpp"

namespace manifold
{
  namespace http
  {
    //================================================================//
    class v2_request_head : public v2_header_block
    {
    private:
    public:
      //----------------------------------------------------------------//
      v2_request_head(v2_header_block&& hb);
      v2_request_head(const request_head& generic_head);
      v2_request_head(const std::string& url = "/", const std::string& method = "get", std::list<hpack::header_field>&& headers = {});
      v2_request_head(const std::string& url, http::method meth, std::list<hpack::header_field>&& headers = {});
      ~v2_request_head();
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      const std::string& method() const;
      void method(const std::string& value);
      void method(http::method value);
      const std::string& path() const;
      void path(const std::string& value);
      const std::string& scheme() const;
      void scheme(const std::string& value);
      const std::string& authority() const;
      void authority(const std::string& value);
      //----------------------------------------------------------------//
    };
    //================================================================//
  }
}

#endif // MANIFOLD_HTTP_V2_REQUEST_HEAD_HPP