
#include "http_router.hpp"
#include "uniform_resource_identifier.hpp"


namespace manifold
{
  namespace http
  {
    //----------------------------------------------------------------//
    void router::register_handler(const std::regex& expression, const std::function<void(server::request&& req, server::response&& res, const std::smatch& matches)>& handler)
    {
      if (handler)
        this->routes_.emplace_back(expression, handler);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void router::register_handler(const std::regex& expression, const std::string& method, const std::function<void(server::request&& req, server::response&& res, const std::smatch& matches)>& handler)
    {
      if (handler)
        this->routes_.emplace_back(expression, handler, method);
    }
    //----------------------------------------------------------------//

    //----------------------------------------------------------------//
    void router::route(server::request&& req, server::response&& res)
    {
      bool both_matched = false;
      bool path_matched = false;

      std::string request_path = percent_decode(uri(req.head().path()).path());
      std::string request_method = req.head().method();
      std::smatch sm;

      for (auto rt = this->routes_.begin(); !both_matched && rt != this->routes_.end(); ++rt)
      {
        if (std::regex_match(request_path, sm, rt->expression))
        {
          if (rt->method.empty() || rt->method == request_method)
          {
            rt->handler(std::move(req), std::move(res), sm);
            both_matched = true;
          }
          path_matched = true;
        }
      }

      if (!both_matched)
      {
        if (path_matched)
        {
          res.head().status_code(status_code::method_not_allowed);
          res.end();
        }
        else
        {
          res.head().status_code(status_code::not_found);
          res.end();
        }
      }
    }
    //----------------------------------------------------------------//
  }
}