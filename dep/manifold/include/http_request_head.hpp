#pragma once

#ifndef MANIFOLD_HTTP_REQUEST_HEAD_HPP
#define MANIFOLD_HTTP_REQUEST_HEAD_HPP

#include "http_message_head.hpp"

namespace manifold
{
  namespace http
  {
    //================================================================//
    std::string basic_auth(const std::string& username, const std::string& password);
    //================================================================//

    //================================================================//
    enum class method { head = 1, get, post, put, del, options, trace, connect, patch };
    std::string method_enum_to_string(method method);
    //================================================================//

    //================================================================//
    class request_head : public header_block
    {
    public:
      //----------------------------------------------------------------//
      request_head(const std::string& path = "/", const std::string& meth = "GET", std::list<std::pair<std::string, std::string>>&& headers = {});
      request_head(const std::string& path, http::method meth, std::list<std::pair<std::string, std::string>>&& headers = {});
      //request_head(class v1_message_head&& v1_headers);
      //request_head(class v2_header_block&& v2_headers);
      request_head(const class v1_request_head& v1_headers);
      request_head(const class v2_request_head& v2_headers);
      ~request_head();
      const std::string& method() const;
      void method(const std::string& value);
      void method(std::string&& value);
      void method(http::method value);
      bool method_is(http::method methodToCheck) const;
      const std::string& path() const;
      void path(const std::string& value);
      const std::string& scheme() const;
      void scheme(const std::string& value);
      const std::string& authority() const;
      void authority(const std::string& value);
      //----------------------------------------------------------------//
    private:
      std::string method_;
      std::string path_;
      std::string authority_;
      std::string scheme_;
    };
    //================================================================//
  };
}

#endif // MANIFOLD_HTTP_REQUEST_HEAD_HPP