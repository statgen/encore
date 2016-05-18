
#include "http_file_transfer.hpp"

#include <chrono>
#include <iomanip>

namespace manifold
{
  namespace http
  {
    //================================================================//
    bool path_exists(const std::string& input_path)
    {
      struct stat s;
      return (stat(input_path.c_str(), &s) == 0);
    }

    bool is_regular_file(const std::string& input_path)
    {
      struct stat s;
      return (stat(input_path.c_str(), &s) == 0 && s.st_mode & S_IFREG);
    }

    bool is_directory(const std::string& input_path)
    {
      struct stat s;
      return (stat(input_path.c_str(), &s) == 0 && s.st_mode & S_IFDIR);
    }

    std::string basename(const std::string& input_path)
    {
      return input_path.substr(input_path.find_last_of("/\\") + 1);
    }

    std::string basename_sans_extension(const std::string& input_path)
    {
      std::string ret = basename(input_path);

      if (ret.size() && ret.front() == '.')
      {
        std::string tmp = ret.substr(1);
        ret = "." + tmp.substr(0, tmp.find_last_of("."));
      }
      else
      {
        ret = ret.substr(0, ret.find_last_of("."));
      }

      return ret;
    }

    std::string extension(const std::string& input_path)
    {
      std::string ret;
      if (input_path.size() && input_path.front() == '.')
      {
        std::string tmp = input_path.substr(1);
        auto pos = tmp.find_last_of(".");
        if (pos != std::string::npos)
          ret = tmp.substr(pos);
      }
      else
      {
        auto pos = input_path.find_last_of(".");
        if (pos != std::string::npos)
          ret = input_path.substr(pos);
      }
      return ret;
    }

    std::string directory(const std::string& input_path)
    {
      std::string ret = input_path;
      if (ret == "." || ret == "..")
        ret += "/";
      ret.erase(ret.find_last_of("/\\") + 1);
      return ret;
    }
    //================================================================//

    //================================================================//
    static const std::map<std::string, std::string> content_type_index =
      {
        {".json", "application/json"},
        {".js",   "application/javascript"},
        {".html", "text/html"},
        {".htm",  "text/html"},
        {".css",  "text/css"},
        {".xml",  "text/xml"},
        {".txt",  "text/plain"},
        {".md",   "text/markdown"}
      };

    std::string content_type_from_extension(const std::string& extension)
    {
      std::string ret;

      auto it = content_type_index.find(extension);
      if (it != content_type_index.end())
        ret = it->second;

      return ret;
    }
    //================================================================//

    //================================================================//
    // xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    const std::uint64_t y[4] = {0x8000000000000000, 0x9000000000000000, 0xa000000000000000, 0xb000000000000000};

    template<typename Rng>
    std::array<std::uint64_t, 2> gen_uuid(Rng& rng)
    {
      std::array<std::uint64_t, 2> ret;

      std::uint32_t r32 = (std::uint32_t) rng();

      std::uint64_t r64_1 = rng();
      r64_1 = r64_1 << 32;

      std::uint64_t r64_2 = rng();
      r64_2 = r64_2 << 32;

      ret[0] = (0xFFFFFFFFFFFF0FFF & (r64_1 | rng())) | 0x4000;
      ret[1] = ((0x0FFFFFFF00000000 & (r64_2 | rng())) | r32) | y[0x03 & r32]; // Should be using a separate rand call to choose index, but this is faster.

      return ret;
    }

    template<typename Rng>
    std::string gen_uuid_str(Rng& rng)
    {
      std::array<std::uint64_t, 2> tmp = gen_uuid(rng);
      std::stringstream ret;
      ret << std::hex << std::setfill('0');
      ret << std::setw(8) << (0xFFFFFFFF & (tmp[0] >> 32));
      ret << "-";
      ret << std::setw(4) << (0xFFFF & (tmp[0] >> 16));
      ret << "-";
      ret << std::setw(4) << (0xFFFF & tmp[0]);
      ret << "-";
      ret << std::setw(4) << (0xFFFF & tmp[1] >> 48);
      ret << "-";
      ret << std::setw(12) << (0xFFFFFFFFFFFF & tmp[1]);
      return ret.str();
    }
    //================================================================//

    //================================================================//
    document_root::document_root(const std::string& path)
      : path_to_root_(path)
    {
      std::seed_seq seed = {(long) (this), (long) std::chrono::high_resolution_clock::now().time_since_epoch().count()};
      this->rng_.seed(seed);
    }

    document_root::~document_root()
    {
    }

    void document_root::reset_root(const std::string& path)
    {
      this->path_to_root_ = path;
    }

    void document_root::add_credentials(const std::string& username, const std::string& password)
    {
      this->user_credentials_[username] = password;
    }

    void document_root::remove_credentials(const std::string& username)
    {
      this->user_credentials_.erase(username);
    }

    void document_root::on_successful_put(const std::function<void(const std::string& file_path)>& cb)
    {
      this->on_put_ = cb;
    }

    void document_root::operator()(server::request&& req, server::response&& res, const std::smatch& matches)
    {
      res.head().header("content-type", "text/plain");

      if (matches.size() < 2)
      {
        // TODO: Handle invalid regex.
      }
      else
      {
        bool authorized = true;

        if (this->user_credentials_.size())
        {
          authorized = false;

          for (auto it = this->user_credentials_.begin(); !authorized && it != this->user_credentials_.end(); ++it)
          {
            if (req.head().header("authorization") == basic_auth(it->first, it->second))
              authorized = true;
          }
        }


        if (!authorized)
        {
          res.head().status_code(status_code::unauthorized);
          res.head().header("WWW-Authenticate", "Basic realm=\"File Transfer\"");
          res.end(status_code_to_reason_phrase(res.head().status_code()));
        }
        else
        {
          std::string path_suffix = matches[1].str();
          std::size_t pos;
          while ((pos = path_suffix.find("..")) != std::string::npos)
          {
            path_suffix.replace(pos, 2, "");
          }

          std::string file_path = this->path_to_root_ + path_suffix;

          if (req.head().method() == "HEAD")
          {
            this->handle_head(std::move(res), file_path);
          }
          else if (req.head().method() == "GET")
          {
            this->handle_get(std::move(res), file_path);
          }
          else if (req.head().method() == "PUT")
          {
            this->handle_put(std::move(req), std::move(res), file_path);
          }
          else
          {
            res.head().status_code(status_code::method_not_allowed);
            res.end(status_code_to_reason_phrase(res.head().status_code()));
          }
        }
      }
    }

    void document_root::handle_head(server::response&& res, const std::string& file_path)
    {
      struct stat st;
      if (stat(file_path.c_str(), &st) != 0 || (st.st_mode & S_IFREG) == 0)
      {
        res.head().status_code(status_code::not_found);
        res.end();
      }
      else
      {
        res.head().header("content-length", std::to_string(st.st_size));
        std::string content_type(content_type_from_extension(extension(file_path)));
        res.head().header("content-type", content_type.size() ? content_type : "application/octet-stream");
#if defined(__APPLE__)
        res.head().header("last-modified", server::date_string(st.st_mtimespec.tv_sec));
#else
        res.head().header("last-modified", server::date_string(st.st_mtime));
#endif

        res.end();
      }
    }

    void document_root::handle_get(server::response&& res, const std::string& file_path)
    {
      struct stat st;
      if (stat(file_path.c_str(), &st) != 0 || (st.st_mode & S_IFREG) == 0)
      {
        res.head().status_code(status_code::not_found);
        res.end(status_code_to_reason_phrase(res.head().status_code()));
      }
      else
      {
        auto ifs = std::make_shared<std::ifstream>(file_path, std::ios::binary);
        if (!ifs->good())
        {
          res.head().status_code(status_code::internal_server_error);
          res.end(status_code_to_reason_phrase(res.head().status_code()));
        }
        else
        {
          auto res_ptr = std::make_shared<server::response>(std::move(res));
          res_ptr->head().header("content-length", std::to_string(st.st_size));
          std::string content_type(content_type_from_extension(extension(file_path)));
          res_ptr->head().header("content-type", content_type.size() ? content_type : "application/octet-stream");
#if defined(__APPLE__)
          res_ptr->head().header("last-modified", server::date_string(st.st_mtimespec.tv_sec));
#else
          res_ptr->head().header("last-modified", server::date_string(st.st_mtime));
#endif

          std::array<char, 4096> buf;
          long bytes_in_buf = ifs->read(buf.data(), buf.size()).gcount();
          if (!ifs->good())
          {
            if (bytes_in_buf > 0)
              res_ptr->end(buf.data(), (std::size_t) bytes_in_buf);
            else
              res_ptr->end();
          }
          else
          {
            res_ptr->on_drain([ifs, res_ptr]()
            {
              std::array<char, 4096> buf;
              long bytes_in_buf = ifs->read(buf.data(), buf.size()).gcount();
              if (bytes_in_buf > 0)
                res_ptr->send(buf.data(), (std::size_t) bytes_in_buf);

              if (!ifs->good())
                res_ptr->end();
            });
            res_ptr->send(buf.data(), (std::size_t) bytes_in_buf);
          }

          res_ptr->on_close([ifs](const std::error_code& ec)
          {
            ifs->close();
          });
        }
      }
    }

    void document_root::handle_put(server::request&& req, server::response&& res, const std::string& file_path)
    {
      std::stringstream ss;
      ss << file_path << "_" << std::to_string(this->rng_()) << ".tmp";
      std::string tmp_file_path(ss.str());
      auto ofs = std::make_shared<std::ofstream>(tmp_file_path, std::ios::binary);

      if (!ofs->good())
      {
        res.head().status_code(status_code::internal_server_error);
        res.end(status_code_to_reason_phrase(res.head().status_code()));
      }
      else
      {
        // TODO: send continue if expected.

        auto res_ptr = std::make_shared<server::response>(std::move(res));


        req.on_data([ofs](const char* const data, std::size_t data_sz)
        {
          ofs->write(data, data_sz);
        });

        req.on_end([res_ptr, ofs, tmp_file_path, file_path, this]()
        {
          if (!ofs->good())
          {
            ofs->close();
            std::remove(tmp_file_path.c_str());
            res_ptr->head().status_code(status_code::internal_server_error);
            res_ptr->end(status_code_to_reason_phrase(res_ptr->head().status_code()));
          }
          else
          {
            ofs->close();
            std::remove(file_path.c_str());
            if (std::rename(tmp_file_path.c_str(), file_path.c_str()) != 0)
            {
              std::remove(tmp_file_path.c_str());
              res_ptr->head().status_code(status_code::internal_server_error);
              res_ptr->end(status_code_to_reason_phrase(res_ptr->head().status_code()));
            }
            else
            {
              res_ptr->end();
              this->on_put_ ? this->on_put_(file_path) : void();
            }
          }
        });

        req.on_close([tmp_file_path, ofs](const std::error_code& ec)
        {
          ofs->close();
          std::remove(tmp_file_path.c_str());
        });
      }
    }
    //================================================================//

    //================================================================//
    const char* file_transfer_error_category_impl::name() const noexcept
    {
      return "Manifold HTTP File Transfer";
    }

    std::string file_transfer_error_category_impl::message(int ev) const
    {
      return "Unknown Error";
    }

    const manifold::http::file_transfer_error_category_impl file_transfer_error_category_object;
    std::error_code make_error_code (manifold::http::file_transfer_errc e)
    {
      return std::error_code(static_cast<int>(e), file_transfer_error_category_object);
    }
    //================================================================//

    //================================================================//
    void file_transfer_client::base_promise_impl::update_progress(std::uint64_t bytes_transferred, std::uint64_t bytes_total)
    {
      on_progress_ ? on_progress_(bytes_transferred, bytes_total) : void();
    }

    void file_transfer_client::base_promise_impl::on_progress(const stream_client::progress_callback& fn)
    {
      on_progress_ = fn;
    }

    void file_transfer_client::base_promise_impl::cancel()
    {
      if (!cancelled_)
      {
        cancelled_ = true;

        on_cancel_ ? on_cancel_() : void();
      }
    }

    void file_transfer_client::base_promise_impl::on_cancel(const std::function<void()>& fn)
    {
      if (cancelled_)
        fn ? fn() : void();
      else
        on_cancel_ = fn;
    }
    //================================================================//

    //================================================================//
    void file_transfer_client::download_promise_impl::fulfill(const std::error_code& ec, const std::string& local_file_path)
    {
      if (!fulfilled_)
      {
        fulfilled_ = true;

        ec_ = ec;
        local_file_path_ = local_file_path;

        on_complete_ ? on_complete_(ec_, local_file_path_) : void();
        on_complete_ = nullptr;
        on_cancel_ = nullptr;
        on_progress_ = nullptr;
      }
    }

    void file_transfer_client::download_promise_impl::on_complete(const std::function<void(const std::error_code&, const std::string&)>& fn)
    {
      if (fulfilled_)
        fn ? fn(ec_, local_file_path_) : void();
      else
        on_complete_ = fn;
    }
    //================================================================//

    //================================================================//
    void file_transfer_client::upload_promise_impl::fulfill(const std::error_code& ec)
    {
      if (!fulfilled_)
      {
        fulfilled_ = true;

        ec_ = ec;

        on_complete_ ? on_complete_(ec_) : void();
        on_complete_ = nullptr;
        on_cancel_ = nullptr;
        on_progress_ = nullptr;
      }
    }

    void file_transfer_client::upload_promise_impl::on_complete(const std::function<void(const std::error_code& ec)>& fn)
    {
      if (fulfilled_)
        fn ? fn(ec_) : void();
      else
        on_complete_ = fn;
    }
    //================================================================//

    //================================================================//
    void file_transfer_client::remote_stat_promise_impl::fulfill(const std::error_code& ec, const statistics& stats)
    {
      if (!fulfilled_)
      {
        fulfilled_ = true;

        ec_ = ec;
        stats_ = stats;

        on_complete_ ? on_complete_(ec_, stats_) : void();
        on_complete_ = nullptr;
        on_cancel_ = nullptr;
        on_progress_ = nullptr;
      }
    }

    void file_transfer_client::remote_stat_promise_impl::on_complete(const std::function<void(const std::error_code&, const statistics&)>& fn)
    {
      if (fulfilled_)
        fn ? fn(ec_, stats_) : void();
      else
        on_complete_ = fn;
    }
    //================================================================//

    //================================================================//
    file_transfer_client::download_promise::download_promise(const std::shared_ptr<download_promise_impl>& impl)
      : impl_(impl)
    {
    }

    file_transfer_client::download_promise& file_transfer_client::download_promise::on_progress(const std::function<void(std::uint64_t, std::uint64_t)>& fn)
    {
      this->impl_->on_progress(fn);
      return *this;
    }

    file_transfer_client::download_promise& file_transfer_client::download_promise::on_complete(const std::function<void(const std::error_code&, const std::string&)>& fn)
    {
      this->impl_->on_complete(fn);
      return *this;
    }

    void file_transfer_client::download_promise::cancel()
    {
      this->impl_->cancel();
    }
    //================================================================//

    //================================================================//
    file_transfer_client::upload_promise::upload_promise(const std::shared_ptr<upload_promise_impl>& impl)
      : impl_(impl)
    {
    }

    void file_transfer_client::upload_promise::on_complete(const std::function<void(const std::error_code&)>& fn)
    {
      this->impl_->on_complete(fn);
    }

    void file_transfer_client::upload_promise::cancel()
    {
      this->impl_->cancel();
    }
    //================================================================//

    //================================================================//
    file_transfer_client::remote_stat_promise::remote_stat_promise(const std::shared_ptr<remote_stat_promise_impl>& impl)
      : impl_(impl)
    {
    }

    void file_transfer_client::remote_stat_promise::on_complete(const std::function<void(const std::error_code&, const file_transfer_client::statistics&)>& fn)
    {
      this->impl_->on_complete(fn);
    }

    void file_transfer_client::remote_stat_promise::cancel()
    {
      this->impl_->cancel();
    }
    //================================================================//

    //================================================================//
    file_transfer_client::file_transfer_client(stream_client& c)
      : stream_client_(c)
    {
      //std::random_device rd;
      auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
      std::uint32_t arr[3] = {(std::uint32_t) (0xFFFFFFFF & (millis >> 32)), (std::uint32_t) std::clock(), (std::uint32_t) (0xFFFFFFFF & millis)};
      std::seed_seq seq(std::begin(arr), std::end(arr));
      this->rng_.seed(seq);
    }

    file_transfer_client::download_promise file_transfer_client::download_file(const uri& remote_source, const std::string& local_destination)
    {
      options ops;
      return download_file(remote_source, local_destination, ops);
    }

    file_transfer_client::download_promise file_transfer_client::download_file(const uri& remote_source, const std::string& local_destination, options ops)
    {
      auto dl_prom = std::make_shared<download_promise_impl>();
      download_promise ret(dl_prom);

      std::string tmp_file_path;
      if (is_directory(local_destination))
      {
        tmp_file_path = local_destination;
        if (tmp_file_path.size() && (tmp_file_path.back() != '/' && tmp_file_path.back() != '\\'))
          tmp_file_path.push_back('/');
      }
      else
      {
        tmp_file_path = directory(local_destination);
      }

      tmp_file_path += (gen_uuid_str(this->rng_) + ".tmp");

      auto dest_ofs = std::make_shared<std::ofstream>(tmp_file_path, std::ios::binary);

      if (!dest_ofs->good())
      {
        dl_prom->fulfill(std::error_code(errno, std::system_category()), "");
      }
      else
      {
        std::list<std::pair<std::string, std::string>> headers;
        if (remote_source.password().size() || remote_source.username().size())
          headers.emplace_back("authorization", basic_auth(remote_source.username(), remote_source.password()));

        auto req_prom = std::make_shared<stream_client::promise>(stream_client_.send_request("GET", remote_source, headers, *dest_ofs));
        dl_prom->on_cancel(std::bind(&stream_client::promise::cancel, req_prom));
        req_prom->on_recv_progress(std::bind(&download_promise_impl::update_progress, dl_prom, std::placeholders::_1, std::placeholders::_2));
        req_prom->on_complete([dl_prom, dest_ofs, local_destination, tmp_file_path, remote_source, ops](const std::error_code& ec, const response_head& headers)
        {
          dest_ofs->close();

          if (ec)
          {
            std::remove(tmp_file_path.c_str());
            dl_prom->fulfill(ec, "");
          }
          else
          {
            std::string local_file_path = local_destination;
            std::replace(local_file_path.begin(), local_file_path.end(), '\\', '/');
            if (is_directory(local_file_path))
            {
              if (local_file_path.size() && local_file_path.back() != '/')
                local_file_path += "/";
              std::string content_disposition = headers.header("content-disposition");
              std::string filename;
              std::regex exp(".*filename=(?:\"([^\"]*)\"|([^\\s;]*)).*", std::regex::ECMAScript);
              std::smatch sm;
              if (std::regex_match(content_disposition, sm, exp))
              {
                if (sm[1].matched)
                  filename = basename(sm[1].str());
                else if (sm[2].matched)
                  filename = basename(sm[2].str());
              }
              else
              {
                filename = basename(remote_source.path());
              }

              if (filename.empty() || filename == "." || filename == "/")
                filename = "file";
              local_file_path += filename;
            }

            std::string destination_file_path = local_file_path;


            if (!ops.replace_existing_file)
            {
              for (std::size_t i = 1; path_exists(destination_file_path); ++i)
              {
                std::stringstream ss;
                ss << directory(local_file_path) << basename_sans_extension(local_file_path) << "_" << i << extension(local_file_path);
                destination_file_path = ss.str();
              }
            }
            else if (is_regular_file(destination_file_path))
            {
              std::remove(destination_file_path.c_str());
            }

            if (std::rename(tmp_file_path.c_str(), destination_file_path.c_str()) != 0)
            {
              std::remove(tmp_file_path.c_str());
              dl_prom->fulfill(std::error_code(errno, std::system_category()), "");
            }
            else
            {
              std::remove(tmp_file_path.c_str());
              dl_prom->fulfill(std::error_code(), destination_file_path);
            }

          }
        });
      }

      return ret;
    }

    file_transfer_client::upload_promise file_transfer_client::upload_file(const std::string& local_source, const uri& remote_destination)
    {
      options ops;
      return upload_file(local_source, remote_destination, ops);
    }

    file_transfer_client::upload_promise file_transfer_client::upload_file(const std::string& local_source, const uri& remote_destination, options ops)
    {
      auto ul_prom = std::make_shared<upload_promise_impl>();
      upload_promise ret(ul_prom);

      auto src_ifs = std::make_shared<std::ifstream>(local_source, std::ios::binary);

      if (!src_ifs->good())
      {
        ul_prom->fulfill(std::error_code(errno, std::system_category()));
      }
      else
      {
        std::list<std::pair<std::string, std::string>> headers;
        if (remote_destination.password().size() || remote_destination.username().size())
          headers.emplace_back("authorization", basic_auth(remote_destination.username(), remote_destination.password()));

        auto resp_entity = std::make_shared<std::stringstream>();
        auto req_prom = std::make_shared<stream_client::promise>(stream_client_.send_request("PUT", remote_destination, headers, *src_ifs, *resp_entity));
        ul_prom->on_cancel(std::bind(&stream_client::promise::cancel, req_prom));
        req_prom->on_send_progress(std::bind(&upload_promise_impl::update_progress, ul_prom, std::placeholders::_1, std::placeholders::_2));
        req_prom->on_complete([ul_prom, src_ifs, resp_entity](const std::error_code& ec, const response_head& headers)
        {
          src_ifs->close();
          ul_prom->fulfill(ec);
        });
      }

      return ret;
    }

    file_transfer_client::remote_stat_promise file_transfer_client::stat_remote_file(const uri& remote_file)
    {
      options ops;
      return stat_remote_file(remote_file, ops);
    }

    file_transfer_client::remote_stat_promise file_transfer_client::stat_remote_file(const uri& remote_file, options ops)
    {
      auto stat_prom = std::make_shared<remote_stat_promise_impl>();
      remote_stat_promise ret(stat_prom);


      std::list<std::pair<std::string, std::string>> headers;
      if (remote_file.password().size() || remote_file.username().size())
        headers.emplace_back("authorization", basic_auth(remote_file.username(), remote_file.password()));

      auto resp_entity = std::make_shared<std::stringstream>();
      auto req_prom = std::make_shared<stream_client::promise>(stream_client_.send_request("HEAD", remote_file, headers, *resp_entity));
      stat_prom->on_cancel(std::bind(&stream_client::promise::cancel, req_prom));
      req_prom->on_complete([stat_prom, resp_entity](const std::error_code& ec, const response_head& headers)
      {
        statistics st;
        st.file_size_known = headers.header_exists("content-length");
        st.file_size = 0;

        std::stringstream ss(headers.header("content-length"));
        ss >> (st.file_size);

        st.mime_type = headers.header("content-type");

        st.modification_date = headers.header("last-modified");

        stat_prom->fulfill(ec, st);
      });

      return ret;
    }
    //================================================================//
  }
}


//    //================================================================//
//    file_download::file_download(asio::io_service &ioservice, const uri& remote_source, const std::string& local_destination, bool replace_existing_file)
//      : url_(remote_source), local_path_(local_destination), replace_existing_file_(replace_existing_file)
//    {
//      std::string path = "/file.txt";
//      auto b = basename(path);
//      auto bse = basename_sans_extension(path);
//      auto e = extension(path);
//      auto d = directory(path);
//      auto i = 1;
//
//      if (remote_source.scheme_name() == "https")
//        c_ = std::unique_ptr<client>(new client(ioservice, remote_source.host(), client::ssl_options(), remote_source.port()));
//      else
//        c_ = std::unique_ptr<client>(new client(ioservice, remote_source.host(), remote_source.port()));
//
//      c_->on_connect([this]()
//      {
//        auto req = this->c_->make_request();
//
//        req.on_response([this](client::response&& res)
//        {
//          if (!res.head().has_successful_status())
//          {
//            // TODO: be more specific.
//            this->err_ = file_transfer_error("HTTP Error (" + http::status_code_to_reason_phrase(res.head().status_code()) + ")");
//          }
//          else
//          {
//            std::string local_file_path = this->local_path_;
//            std::replace(local_file_path.begin(), local_file_path.end(), '\\', '/');
//            if (is_directory(local_file_path))
//            {
//              if (local_file_path.size() && local_file_path.back() != '/')
//                local_file_path += "/";
//              std::string content_disposition = res.head().header("content-disposition");
//              std::string filename;
//              std::regex exp(".*filename=(?:\"([^\"]*)\"|([^\\s;]*)).*", std::regex::ECMAScript);
//              std::smatch sm;
//              if (std::regex_match(content_disposition, sm, exp))
//              {
//                if (sm[1].matched)
//                  filename = basename(sm[1].str());
//                else if (sm[2].matched)
//                  filename = basename(sm[2].str());
//              }
//              else
//              {
//                filename = basename(this->url_.path());
//              }
//
//              if (filename.empty() || filename == "." || filename == "/")
//                filename = "file";
//              local_file_path += filename;
//            }
//
//            std::string destination_file_path = local_file_path;
//
//
//
//            if(!this->replace_existing_file_)
//            {
//              for(std::size_t i = 1; path_exists(destination_file_path); ++i)
//              {
//                std::stringstream ss;
//                ss << directory(local_file_path) << basename_sans_extension(local_file_path) << "_" << i << extension(local_file_path);
//                destination_file_path = ss.str();
//              }
//            }
//            else if(is_regular_file(destination_file_path))
//            {
//              std::remove(destination_file_path.c_str());
//            }
//
//            this->file_.open(destination_file_path, std::ios::binary);
//
//            if (!this->file_.good())
//            {
//              this->err_ = file_transfer_error("Could Not Open File For Writing");
//              res.close(http::errc::cancel);
//            }
//            else
//            {
//              this->result_ = destination_file_path;
//              res.on_data([this](const char*const data, std::size_t data_sz)
//              {
//                this->file_.write(data, data_sz);
//              });
//
////              res.on_end([]()
////              {
////              });
//            }
//          }
//        });
//
//        req.on_close(std::bind(&file_download::on_close_handler, this, std::placeholders::_1));
//
//        req.head().path(this->url_.path_with_query());
//        req.end();
//      });
//
//      this->c_->on_close(std::bind(&file_download::on_close_handler, this, std::placeholders::_1));
//    }
//
//    file_download::~file_download()
//    {
//      this->cancel();
//      assert(!this->file_.is_open());
//    }
//
//    void file_download::on_close_handler(errc ec)
//    {
//      if (!this->completed_)
//      {
//        this->completed_ = true;
//
//        this->file_.close();
//
//        if (ec != errc::no_error)
//        {
//          if (this->result_.size())
//            std::remove(this->result_.c_str());
//
//          if (!err_)
//          {
//            this->err_ = file_transfer_error("Connection Closed Prematurely (" + std::to_string((unsigned) ec) + ")");
//          }
//        }
//
//        this->on_complete_ ? this->on_complete_(this->err_, this->result_) : void();
//        this->on_complete_ = nullptr;
//        this->c_->close();
//      }
//    }
//
//    void file_download::on_complete(std::function<void(const file_transfer_error& err, const std::string& local_file_path)>&& cb)
//    {
//      if (this->completed_)
//        cb(err_, result_);
//      else
//        on_complete_ = std::move(cb);
//    }
//
//    void file_download::on_complete(const std::function<void(const file_transfer_error &err, const std::string &local_file_path)>& cb)
//    {
//      this->on_complete(std::function<void(const file_transfer_error &err, const std::string &local_file_path)>(cb));
//    }
//
//    void file_download::on_progress(std::function<void(std::uint64_t transfered, std::uint64_t total)> &&cb)
//    {
//      on_progress_ = std::move(cb);
//    }
//
//    void file_download::on_progress(const std::function<void(std::uint64_t transfered, std::uint64_t total)> &cb)
//    {
//      this->on_progress(std::function<void(std::uint64_t transfered, std::uint64_t total)>(cb));
//    }
//
//    void file_download::cancel()
//    {
//      this->c_->close();
//    }
//    //================================================================//