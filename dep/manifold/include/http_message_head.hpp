#pragma once

#ifndef MANIFOLD_HTTP_MESSAGE_HEAD_HPP
#define MANIFOLD_HTTP_MESSAGE_HEAD_HPP

#include <string>
#include <list>

namespace manifold
{
  namespace http
  {
    //================================================================//
    class header_block
    {
    public:
      //----------------------------------------------------------------//
      header_block();
      header_block(const class v1_header_block& v1_headers);
      header_block(const class v2_header_block& v2_headers);
      header_block(std::list<std::pair<std::string,std::string>>&& headers);
      virtual ~header_block();
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      bool header_exists(const std::string& name) const;
      bool header_exists(std::string&& name) const;
      void header(const std::string& name, const std::string& value);
      void header(std::string&& name, std::string&& value);
      void multi_header(const std::string& name, const std::list<std::string>& values);
      void multi_header(std::string&& name, std::list<std::string>&& values);
      const std::string& header(const std::string& name) const;
      const std::string& header(std::string&& name) const;
      std::list<std::string> multi_header(const std::string& name) const;
      const std::list<std::pair<std::string,std::string>>& raw_headers() const;
      std::size_t size() const;
      //----------------------------------------------------------------//
    protected:
      //----------------------------------------------------------------//
      std::list<std::pair<std::string,std::string>> headers_;
      //----------------------------------------------------------------//
    };
    //================================================================//
  }
}

#endif // MANIFOLD_HTTP_MESSAGE_HEAD_HPP