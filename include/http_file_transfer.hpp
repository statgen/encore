#pragma once

#ifndef MANIFOLD_HTTP_FILE_TRANSFER_HPP
#define MANIFOLD_HTTP_FILE_TRANSFER_HPP

#include "http_stream_client.hpp"
#include "http_server.hpp"

#include <regex>
#include <fstream>
#include <random>

namespace manifold
{
  namespace http
  {
    enum class authentication
    {
      basic = 0
    };

    class document_root
    {
    public:
      document_root(const std::string& path = "");
      ~document_root();
      void operator()(server::request&& req, server::response&& res, const std::smatch& matches);
      void reset_root(const std::string& path = "");
      void add_credentials(const std::string& username, const std::string& password);
      void remove_credentials(const std::string& username);
      void on_successful_put(const std::function<void(const std::string& file_path)>& cb);
    private:
      std::string path_to_root_;
      std::map<std::string, std::string> user_credentials_;
      std::minstd_rand rng_;
      std::function<void(const std::string& file_path)> on_put_;

      void handle_head(server::response&& res, const std::string& file_path);
      void handle_get(server::response&& res, const std::string& file_path);
      void handle_put(server::request&& req, server::response&& res, const std::string& file_path);
    };


    enum class file_transfer_errc
    {

    };

    std::error_code make_error_code(manifold::http::file_transfer_errc e);
  }
}

namespace std
{
  template<> struct is_error_code_enum<manifold::http::file_transfer_errc> : public true_type {};
}

namespace manifold
{
  namespace http
  {
    class file_transfer_error_category_impl : public std::error_category
    {
    public:
      file_transfer_error_category_impl() {}
      ~file_transfer_error_category_impl() {}
      const char* name() const noexcept;
      std::string message(int ev) const;
    };


    class file_transfer_client
    {
    public:
      struct statistics
      {
        bool file_size_known;
        std::uint64_t file_size;
        std::string mime_type;
        std::string modification_date;
        // TODO: cache expire
      };
    private:
      class base_promise_impl
      {
      public:
        void cancel();
        void on_cancel(const std::function<void()>&);
        void update_progress(std::uint64_t, std::uint64_t);
        void on_progress(const stream_client::progress_callback&);
      protected:
        bool cancelled_ = false;
        std::function<void()> on_cancel_;
        stream_client::progress_callback on_progress_;
      };

      class download_promise_impl : public base_promise_impl
      {
      public:
        void fulfill(const std::error_code& ec, const std::string& local_file_path);
        void on_complete(const std::function<void(const std::error_code& ec, const std::string& local_file_path)>& fn);
      private:
        bool fulfilled_ = false;
        std::function<void(const std::error_code&, const std::string&)> on_complete_;
        std::string local_file_path_;
        std::error_code ec_;
      };

      class upload_promise_impl : public base_promise_impl
      {
      public:
        void fulfill(const std::error_code& ec);
        void on_complete(const std::function<void(const std::error_code& ec)>& fn);
      private:
        bool fulfilled_ = false;
        std::function<void(const std::error_code&)> on_complete_;
        std::error_code ec_;
      };

      class remote_stat_promise_impl : public base_promise_impl
      {
      public:
        void fulfill(const std::error_code& ec, const statistics& stats);
        void on_complete(const std::function<void(const std::error_code& ec, const statistics& stats)>& fn);
      private:
        bool fulfilled_ = false;
        std::function<void(const std::error_code&, const statistics&)> on_complete_;
        statistics stats_;
        std::error_code ec_;
      };
    public:
      file_transfer_client(stream_client& c);

      class download_promise
      {
      public:
        download_promise(const std::shared_ptr<download_promise_impl>& impl);
        download_promise& on_progress(const std::function<void(std::uint64_t bytes_transferred, std::uint64_t bytes_total)>& fn);
        download_promise& on_complete(const std::function<void(const std::error_code& ec, const std::string& local_file_path)>& fn);
        void cancel();
      private:
        std::shared_ptr<download_promise_impl> impl_;
      };

      class upload_promise
      {
      public:
        upload_promise(const std::shared_ptr<upload_promise_impl>& impl);
        void on_complete(const std::function<void(const std::error_code& ec)>& fn);
        void cancel();
      private:
        std::shared_ptr<upload_promise_impl> impl_;
      };

      class remote_stat_promise
      {
      public:
        remote_stat_promise(const std::shared_ptr<remote_stat_promise_impl>& impl);
        void on_complete(const std::function<void(const std::error_code& ec, const statistics& stats)>& fn);
        void cancel();
      private:
        std::shared_ptr<remote_stat_promise_impl> impl_;
      };



      struct options
      {
        bool replace_existing_file = false; // Download only.
        authentication auth_type = authentication::basic;
      };

      download_promise download_file(const uri& remote_source, const std::string& local_destination);
      download_promise download_file(const uri& remote_source, const std::string& local_destination, options ops);
      upload_promise upload_file(const std::string& local_source, const uri& remote_destination);
      upload_promise upload_file(const std::string& local_source, const uri& remote_destination, options ops);
      remote_stat_promise stat_remote_file(const uri& remote_file);
      remote_stat_promise stat_remote_file(const uri& remote_file, options ops);
    private:
      stream_client& stream_client_;
      std::mt19937 rng_;
    };

//    class file_transfer_error
//    {
//    public:
//      file_transfer_error() {}
//      file_transfer_error(const std::string& msg)
//        : message_(msg)
//      {
//      }
//      virtual ~file_transfer_error() {}
//
//      operator bool() const { return (message_.size() > 0); }
//      const std::string& message() const { return this->message_; }
//    private:
//      std::string message_;
//    };
//
//    class file_upload
//    {
//    };
//
//    class file_download
//    {
//    public:
//      file_download(asio::io_service& ioservice, const uri& remote_source, const std::string& local_destination, bool replace_existing_file = false);
//      ~file_download();
//
//      void on_complete(std::function<void(const file_transfer_error& err, const std::string& local_file_path)>&& cb);
//      void on_complete(const std::function<void(const file_transfer_error& err, const std::string& local_file_path)>& cb);
//      void on_progress(std::function<void(std::uint64_t transfered, std::uint64_t total)>&& cb);
//      void on_progress(const std::function<void(std::uint64_t transfered, std::uint64_t total)>& cb);
//      void cancel();
//    private:
//      std::unique_ptr<client> c_;
//      std::ofstream file_;
//      const uri url_;
//      const std::string local_path_;
//      const bool replace_existing_file_;
//
//      void on_close_handler(errc ec);
//      std::function<void(std::uint64_t transfered, std::uint64_t total)> on_progress_;
//      std::function<void(const file_transfer_error& err, const std::string& local_file_path)> on_complete_;
//      bool completed_ = false;
//      file_transfer_error err_;
//      std::string result_;
//    };
//
//    class file_check
//    {
//    };
  }
}

#endif //MANIFOLD_HTTP_FILE_TRANSFER_HPP