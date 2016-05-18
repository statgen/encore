
#include <memory>
#include <fstream>
#include <cstdio>
#include <iomanip>
#include <tuple>
#include <thread>
#include <future>

#include "asio.hpp"
#include "http_server.hpp"
#include "http_client.hpp"
#include "http_router.hpp"
#include "hpack.hpp"
#include "http_file_transfer.hpp"




using namespace manifold;

class move_only
{
public:
  move_only() { }
  move_only(move_only&& source) { }
  move_only& operator=(move_only&& source) { return *this; }
private:
  move_only(const move_only&) = delete;
  move_only& operator=(const move_only&) = delete;
};

std::tuple<move_only, move_only, int> return_move_only()
{
  static move_only ret;
  static move_only ret2;
  return std::make_tuple(std::move(ret), std::move(ret2), 5);
}
//================================================================//
void handle_push_promise(http::client::request && req, std::uint32_t dependency_stream_id)
{

  req.on_response([](http::client::response && resp)
  {
    for (auto it : resp.head().raw_headers())
      std::cout << it.first << ": " << it.second << std::endl;

    resp.on_data([](const char*const d, std::size_t sz)
    {
      std::cout << std::string(d, sz) << std::endl;
    });
  });
}
//================================================================//

//################################################################//
int main()
{
  move_only first;
  move_only second;
  int third;
  std::tie(first, second, third) = return_move_only();

  asio::io_service ioservice;
  asio::ssl::context client_ssl_ctx(asio::ssl::context::tlsv12);
  asio::ssl::context server_ssl_ctx(asio::ssl::context::tlsv12);

//  http::user_agent ua(ioservice);
//  auto r = ua.send_request("POST", uri("https://127.0.0.1:8080/foo"), std::stringstream("FoooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooBAR!"));
//  r.on_response([](http::client::response&& resp)
//  {
//    std::cout << "status: " << resp.head().status_code() << std::endl;
//    auto resp_entity = std::make_shared<std::stringstream>();
//    resp.on_data([resp_entity](const char*const data, std::size_t data_sz)
//    {
//      resp_entity->write(data, data_sz);
//    });
//
//    resp.on_end([resp_entity]()
//    {
//      std::cout << resp_entity->str() << "[DONE]" << std::endl;
//    });
//  });
//
//  auto r2 = ua.send_request("POST", uri("https://127.0.0.1:8080/foo"), std::stringstream("FoooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooBAR!"));
//  r2.on_response([](http::client::response&& resp)
//  {
//    std::cout << "status: " << resp.head().status_code() << std::endl;
//    auto resp_entity = std::make_shared<std::stringstream>();
//    resp.on_data([resp_entity](const char*const data, std::size_t data_sz)
//    {
//      resp_entity->write(data, data_sz);
//    });
//
//    resp.on_end([resp_entity]()
//    {
//      std::cout << resp_entity->str() << "[DONE2]" << std::endl;
//    });
//  });
//
//  auto r3 = ua.send_request("POST", uri("https://127.0.0.1:8080/foo"), std::stringstream("FoooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooBAR!"));
//  r3.on_response([](http::client::response&& resp)
//  {
//    std::cout << "status: " << resp.head().status_code() << std::endl;
//    auto resp_entity = std::make_shared<std::stringstream>();
//    resp.on_data([resp_entity](const char*const data, std::size_t data_sz)
//    {
//      resp_entity->write(data, data_sz);
//    });
//
//    resp.on_end([resp_entity]()
//    {
//      std::cout << resp_entity->str() << "[DONE3]" << std::endl;
//    });
//  });

//  //----------------------------------------------------------------//
//  std::uint32_t plus_sign_code = (0x7fb << (32 - 11));
//  auto res = hpack::huffman_code_tree.find(hpack::huffman_code(plus_sign_code, 32));
//  if (res != hpack::huffman_code_tree.end())
//    std::cout << res->second << std::endl;
//
//  std::string compressed_literal = {(char)0xf1,(char)0xe3,(char)0xc2,(char)0xe5,(char)0xf2,(char)0x3a,(char)0x6b,(char)0xa0,(char)0xab,(char)0x90,(char)0xf4,(char)0xff};
//  for (auto it = compressed_literal.begin(); it != compressed_literal.end(); ++it)
//    std::cout << std::hex << (unsigned int)(std::uint8_t)(*it) << std::dec << std::endl;
//  std::string uncompressed_literal;
//  hpack::decoder::huffman_decode(compressed_literal.begin(), compressed_literal.end(), uncompressed_literal);
//  std::cout << "uncompressed size: " << uncompressed_literal.size() << std::endl;
//  std::cout << "uncompressed value: " << uncompressed_literal << std::endl;
//
////  for (auto it = hpack::huffman_code_tree.begin(); it != hpack::huffman_code_tree.end(); ++it)
////    std::cout << "- " << std::hex << it->first.msb_code << std::dec << " | " << it->second << std::endl;
//
//  std::cout.flush();
//
//  //----------------------------------------------------------------//
//
//  //----------------------------------------------------------------//
//  // HPack Test
//  //
//  std::size_t http2_default_table_size = 4096;
//  hpack::encoder enc(http2_default_table_size);
//  hpack::decoder dec(http2_default_table_size);
//
//  std::list<hpack::header_field> send_headers{
//    {":path","/"},
//    {":method","GET"},
//    {"content-type","application/json; charset=utf8"},
//    {"content-length","30"},
//    {"custom-header","foobar; baz"},
//    {"custom-header2","NOT INDEXED", hpack::cacheability::no}};
//  std::list<hpack::header_field> send_headers2{
//    {":path","/"},
//    {":method","GET"},
//    {"custom-header","foobar; baz3"},
//    {"custom-header2","NOT INDEXED", hpack::cacheability::never}};
//
//  std::list<hpack::header_field> recv_headers;
//  std::list<hpack::header_field> recv_headers2;
//
//  std::string serialized_headers;
//  enc.encode(send_headers, serialized_headers);
//  dec.decode(serialized_headers.begin(), serialized_headers.end(), recv_headers);
//
//  for (auto it : recv_headers)
//    std::cout << it.name << ": " << it.value << std::endl;
//  std::cout << std::endl;
//
//  // Encoders can use table size updates to clear dynamic table.
//  enc.add_table_size_update(0);
//  enc.add_table_size_update(4096);
//
//  serialized_headers = "";
//  enc.encode(send_headers2, serialized_headers);
//  dec.decode(serialized_headers.begin(), serialized_headers.end(), recv_headers2);
//  for (auto it : recv_headers2)
//    std::cout << it.name << ": " << it.value << std::endl;
  //----------------------------------------------------------------//

  //----------------------------------------------------------------//
  // Server Test
  //
  http::router app;

  http::document_root get_doc_root("./");
  get_doc_root.add_credentials("user", "pass");
  app.register_handler(std::regex("^/files/(.*)$"), "HEAD", http::document_root("./"));
  app.register_handler(std::regex("^/files/(.*)$"), "GET", std::ref(get_doc_root));
  app.register_handler(std::regex("^/files/(.*)$"), "PUT", http::document_root("./"));
  get_doc_root.add_credentials("user", "password");

  app.register_handler(std::regex("^/redirect-url$"), [](http::server::request&& req, http::server::response&& res, const std::smatch& matches)
  {
    res.head().status_code(http::status_code::found);
    res.head().header("location","/new-url");
    res.end();
  });

  app.register_handler(std::regex("^/(.*)$"), [](http::server::request&& req, http::server::response&& res, const std::smatch& matches)
  {
    auto res_ptr = std::make_shared<http::server::response>(std::move(res));

    for (auto it : req.head().raw_headers())
      std::cout << it.first << ": " << it.second << std::endl;

    auto req_entity = std::make_shared<std::stringstream>();
    req.on_data([req_entity](const char*const data, std::size_t datasz)
    {
      req_entity->write(data, datasz);
    });

    req.on_end([res_ptr, req_entity]()
    {
      auto push_promise = res_ptr->send_push_promise(http::request_head("/push-url"));

      res_ptr->send("Received: " + req_entity->str());
      res_ptr->end();

      push_promise.fulfill([](http::server::request&& rq, http::server::response&& rs)
      {
        // TODO: have on_end immidiately callback if closed or half closed remote.
        rs.end("Here's the promised data.");
      });

    });

    req.on_close([](const std::error_code& e)
    {
      std::cout << "on_close called on server" << std::endl;
    });

  });


//  auto ops = http::server::ssl_options(asio::ssl::context::method::sslv23);
//  {
//    std::ifstream ifs("/Users/jonathonl/Developer/certs/server.key");
//    if (ifs.good())
//      ops.key.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
//  }
//  {
//    std::ifstream ifs("/Users/jonathonl/Developer/certs/server.crt");
//    if (ifs.good())
//      ops.cert.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
//  }
//  {
//    std::ifstream ifs("/Users/jonathonl/Developer/certs/ca.crt");
//    if (ifs.good())
//      ops.ca.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
//  }

  std::vector<char> chain;
  std::vector<char> key;
  std::vector<char> dhparam;
  {
    std::ifstream ifs("tests/certs/server.crt");
    if (ifs.good())
      chain.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
  }
  {
    std::ifstream ifs("tests/certs/server.key");
    if (ifs.good())
      key.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
  }
  {
    std::ifstream ifs("tests/certs/dh2048.pem");
    if (ifs.good())
      dhparam.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
  }

  server_ssl_ctx.use_certificate_chain(asio::buffer(chain.data(), chain.size()));
  server_ssl_ctx.use_private_key(asio::buffer(key.data(), key.size()), asio::ssl::context::pem);
  server_ssl_ctx.use_tmp_dh(asio::buffer(dhparam.data(), dhparam.size()));

  http::server srv(ioservice, server_ssl_ctx, 8080);
  srv.reset_timeout(std::chrono::seconds(15));
  srv.listen(std::bind(&http::router::route, &app, std::placeholders::_1, std::placeholders::_2));

  //http::server ssl_srv(ioservice, http::server::ssl_options(asio::ssl::context::method::sslv23), 8081, "0.0.0.0");
  //ssl_srv.listen(std::bind(&http::router::route, &app, std::placeholders::_1, std::placeholders::_2));
  //----------------------------------------------------------------//

  if (true)
  {
    //----------------------------------------------------------------//
    // Client to Local Server Test
    //
    http::client agnt(ioservice, client_ssl_ctx);
    http::stream_client stream_agnt(agnt);
    http::file_transfer_client file_transfer_agnt(stream_agnt);

    agnt.reset_timeout(std::chrono::seconds(5));

    http::file_transfer_client::options ops;
    ops.replace_existing_file = true;
    auto t = std::chrono::system_clock::now().time_since_epoch();
    std::cout << "Starting Download ..." << std::endl;
    auto last_percent = std::make_shared<std::uint64_t>(0);

    {
      auto p = file_transfer_agnt.download_file(uri("https://user:password@localhost:8080/files/test_cmp.rfcmp"), "./")
      .on_progress([last_percent](std::uint64_t transferred, std::uint64_t total)
      {
        if (total)
        {
          std::uint64_t percent = (static_cast<double>(transferred) / static_cast<double>(total)) * 100;
          if (percent > *last_percent)
          {
            *last_percent = percent;
            std::cout << "\r" << percent << "%" << std::flush;
          }
        }
      }).on_complete([t](const std::error_code& ec, const std::string& file_path)
      {
        std::cout << std::endl;
        if (ec)
        {
          std::cout << ec.message() << std::endl;
        }
        else
        {
          std::cout << "DL SUCCEEDED" << std::endl;
          std::cout << "SECONDS: " << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch() - t).count() << std::endl;
        }
      });
    }



    for (size_t i = 0; i < 0; ++i)
    {
      file_transfer_agnt.download_file(uri("https://user:password@localhost:8080/files/readme.md"), "./").on_complete([i](const std::error_code& ec, const std::string& file_path)
      {
        if (ec)
        {
          std::cout << ec.message() << std::endl;
        }
        else
        {
          std::cout << "GET " << std::setfill('0') << std::setw(2) << i;
          std::cout << " SUCCEEDED" << std::endl;
        }
      });
    }

//    std::thread([&client_ioservice]()
//    {
//      client_ioservice.run();
//      auto a = 0;
//    }).detach();
    ioservice.run();
    return 0;

    auto post_data = std::make_shared<std::stringstream>("name=value&foo=bar");
    auto post_resp_entity = std::make_shared<std::stringstream>();
    auto post = stream_agnt.send_request("POST", uri("http://localhost:8080/redirect-url"), {{"content-type","application/x-www-form-urlencoded"}}, *post_data, *post_resp_entity);
    post.on_complete([post_resp_entity, post_data](const std::error_code& ec, const http::response_head& headers)
    {
      if (ec)
      {
        std::cout << ec.message() << std::endl;
      }
      else
      {
        std::cout << "status: " << headers.status_code() << std::endl;
        std::cout << post_resp_entity->str() << std::endl;
      }
    });

    ioservice.run();

    agnt.make_request("localhost", 8080, [](const std::error_code& ec, http::client::request&& req)
    {
      if (ec)
      {
        std::cout << ec.message() << std::endl;
      }
      else
      {
        req.head() = http::request_head("/foobar", http::method::post,
          {
            {"content-type", "application/x-www-form-urlencoded"}
          });

        req.on_response([](http::client::response&& resp)
        {
          for (auto it : resp.head().raw_headers())
            std::cout << it.first << ": " << it.second << std::endl;

          if (!resp.head().has_successful_status())
          {
            resp.cancel();
          }
          else
          {
            auto response_data = std::make_shared<std::stringstream>("");
            resp.on_data([response_data](const char* const data, std::size_t datasz)
            {
              response_data->write(data, datasz);
            });

            resp.on_end([response_data]()
            {
              if (response_data->rdbuf()->in_avail())
                std::cout << response_data->rdbuf() << std::endl;
            });
          }
        });

        req.on_push_promise(std::bind(handle_push_promise, std::placeholders::_1, req.stream_id()));

        req.on_close([](const std::error_code& e)
        {
          std::cout << "on_close called on client" << std::endl;
        });

        req.send(std::string("0123456789name=value&name2=value298765432100123456789name=value&name2=value298765432100123456789name=value&name2=value298765432100123456789name=value&name2=value298765432100123456789name=value&name2=value29876543210\r\n"));
        req.send(std::string("0123456789name=value&name2=value298765432100123456789name=value&name2=value298765432100123456789name=value&name2=value298765432100123456789name=value&name2=value298765432100123456789name=value&name2=value29876543210\r\n"));
        req.send(std::string("0123456789name=value&name2=value298765432100123456789name=value&name2=value298765432100123456789name=value&name2=value298765432100123456789name=value&name2=value298765432100123456789name=value&name2=value29876543210\r\n"));
        req.end(std::string("0123456789name=value&name2=value298765432100123456789name=value&name2=value298765432100123456789name=value&name2=value298765432100123456789name=value&name2=value298765432100123456789name=value&name2=value29876543210\r\n"));
      }
    });
    //----------------------------------------------------------------//
  }


//  //----------------------------------------------------------------//
//  // Client to Google Test
//  //
//  else if (false)
//  {
//    http::client c2(ioservice, "www.google.com", http::client::ssl_options());
//    c2.on_connect([&c2]()
//    {
//      if (true)
//      {
//        auto request = std::make_shared<http::client::request>(c2.make_request());
//
//        request->on_response([request, &c2](http::client::response && resp)
//        {
//          std::cout << resp.head().status_code() << std::endl;
//          for (auto it : resp.head().raw_headers())
//            std::cout << it.first << ": " << it.second << std::endl;
//
//          auto response = std::make_shared<http::client::response>(std::move(resp));
//
//
//          response->on_data([](const char *const data, std::size_t datasz)
//          {
//            std::cout << std::string(data, datasz);
//          });
//
//          response->on_end([]()
//          {
//            std::cout << std::endl << "DONE" << std::endl;
//          });
//        });
//
//        request->on_close([&c2](http::errc error_code)
//        {
//          std::cout << "client request on_close: " << error_code << std::endl;
//        });
//
//        request->head().header("connection","keep-alive");
//        request->end();
//
////        request->head().path("/foobar");
////        request->head().method(http::method::post);
////        request->head().header("content-type","application/x-www-form-urlencoded");
////        request->head().header("accept","text/html,application/json,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
////        request->end("name=value&name2=value2");
//
//
//      }
//
//      if (false)
//      {
//        auto req2(c2.make_request());
//
//
//        auto req_ptr = std::make_shared<http::client::request>(std::move(req2));
//
//        // Create file stream for response.
//        auto ofs = std::make_shared<std::ofstream>("./reponse_file.txt.tmp");
//
//        // Set on response handler.
//        req_ptr->on_response([&ofs, req_ptr](http::client::response &&res)
//        {
//          auto res_ptr = std::make_shared<http::client::response>(std::move(res));
//
//          if (res.head().status_code() != 200)
//          {
//            req_ptr->close();
//          }
//          else
//          {
//            // Write response data to file.
//            res_ptr->on_data([ofs](const char *const data, std::size_t datasz)
//            {
//              ofs->write(data, datasz);
//            });
//
//            // Close and rename file when done.
//            res_ptr->on_end([ofs]()
//            {
//              ofs->close();
//              std::rename("./response_file.txt.tmp","./resonse_file.txt");
//            });
//          }
//        });
//
//        //
//        req_ptr->on_close([ofs](http::errc ec)
//        {
//          ofs->close();
//          std::remove("./response_file.txt.tmp");
//        });
//
//        req_ptr->on_push_promise(std::bind(handle_push_promise, std::placeholders::_1, req_ptr->stream_id()));
//
//        req_ptr->end();
//      }
//    });
//
//    c2.on_close([](http::errc ec)
//    {
//      std::cerr << ec << std::endl;
//    });
//    ioservice.run();
//  }
//  //----------------------------------------------------------------//

  ioservice.run();
}
//################################################################//