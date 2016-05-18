//
// Created by Jonathon LeFaive on 1/3/16.
//

#ifndef MANIFOLD_HTTP_V1_HEADER_BLOCK_HPP
#define MANIFOLD_HTTP_V1_HEADER_BLOCK_HPP

#include <string>
#include <list>
#include <iostream>

#include "http_message_head.hpp"

namespace manifold
{
  namespace http
  {
    //================================================================//
    class v1_header_block
    {
    public:
      //----------------------------------------------------------------//
      v1_header_block();
      v1_header_block(const header_block& generic_head);
      virtual ~v1_header_block();
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      bool header_exists(const std::string& name) const;
      bool header_exists(std::string&& name) const;
      void remove_header(const std::string& name);
      void remove_header(std::string&& name);
      void header(const std::string& name, const std::string& value);
      void header(std::string&& name, std::string&& value);
      void multi_header(const std::string& name, const std::list<std::string>& values);
      void multi_header(std::string&& name, std::list<std::string>&& values);
      const std::string& header(const std::string& name) const;
      std::list<std::string> multi_header(const std::string& name) const;
      const std::list<std::pair<std::string, std::string>>& raw_headers() const;
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      bool empty() const { return this->headers_.empty(); }
      std::size_t size() const { return this->headers_.size(); }
      //----------------------------------------------------------------//

      //----------------------------------------------------------------//
      static void serialize(const v1_header_block& source, std::ostream& destination);
      static bool deserialize(std::istream& source, v1_header_block& destination);
      //----------------------------------------------------------------//
    protected:
      //----------------------------------------------------------------//
      std::list<std::pair<std::string,std::string>> headers_;
      //----------------------------------------------------------------//
    };

    class v1_message_head : public v1_header_block
    {
    public:
      //----------------------------------------------------------------//
      static void serialize(const v1_message_head& source, std::ostream& destination);
      static bool deserialize(std::istream& source, v1_message_head& destination);
      //----------------------------------------------------------------//
    protected:
      virtual bool start_line(std::string&& value) = 0;
      virtual std::string start_line() const = 0;
    };
    //================================================================//
  }
}

#endif //MANIFOLD_HTTP_V1_HEADER_BLOCK_HPP
