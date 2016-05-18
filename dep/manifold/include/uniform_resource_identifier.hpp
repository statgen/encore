#pragma once

#ifndef MANIFOLD_HTTP_UNIFORM_RESOURCE_IDENTIFIER_HPP
#define MANIFOLD_HTTP_UNIFORM_RESOURCE_IDENTIFIER_HPP

#include <string>

namespace manifold
{
  std::string encode_uri(const std::string&);
  std::string encode_uri_component(const std::string&);
  std::string percent_decode(const std::string&);

  class uri
  {
  private:
    void parse_uri(const std::string& string_uri);
    void parse_authority(const std::string& authority);
  protected:
    std::string scheme_name_;
    std::string host_;
    unsigned short port_ = 0;
    std::string path_;
    std::string query_;
    std::string fragment_;
    std::string username_;
    std::string password_;
  public:
    uri() {}

    uri(const std::string& string_uri)
    {
      this->parse_uri(string_uri);
    }

    virtual ~uri() {}

    const std::string& username() const { return this->username_; }
    const std::string& password() const { return this->password_; }
    const std::string& scheme_name() const { return this->scheme_name_; }
    const std::string& host() const { return this->host_; }
    unsigned short port() const { return this->port_; }
    const std::string& path() const { return this->path_; }
    const std::string& query() const { return this->query_; }
    const std::string& fragment() const { return this->fragment_; }
    bool is_relative() const { return (this->host_.empty() && this->path_.size()); }
    bool is_valid() const { return (this->host_.size() || this->path_.size()); }


    std::string basename() const;
    std::string socket_address() const;
    std::string authority() const;
    std::string to_string() const;
    std::string path_with_query() const;

    void scheme_name(const std::string& value) { this->scheme_name_ = value; }
    void host(const std::string& value) { this->host_ = value; }
    void port(unsigned short value) { this->port_ = value; }
    void path(const std::string& value) { this->path_ = value; }
    void query(const std::string& value) { this->query_ = value; }
    void fragment(const std::string& value) { this->fragment_ = value; }
    void username(const std::string& value) { this->username_ = value; }
    void password(const std::string& value) { this->password_ = value; }
  };
}

#endif //MANIFOLD_HTTP_UNIFORM_RESOURCE_IDENTIFIER_HPP