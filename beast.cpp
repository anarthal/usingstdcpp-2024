
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>

#include <exception>
#include <iostream>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

asio::awaitable<void> handle_request_impl()
{
    // Coroutines know which executor are using
    auto ex = co_await asio::this_coro::executor;

    // I/O objects
    asio::ip::tcp::socket sock(ex);
    asio::ip::tcp::resolver resolv(ex);

    // Resolve the hostname and port into a set of endpoints
    auto endpoints = co_await resolv.async_resolve("python.org", "80", asio::deferred);

    // Connect to the server
    co_await asio::async_connect(sock, endpoints, asio::deferred);

    // Compose and write the request
    http::request<http::string_body> req{http::verb::get, "/", 11};
    req.set(http::field::host, "python.org");
    req.set(http::field::user_agent, "Beast");
    co_await http::async_write(sock, req, asio::deferred);

    // Read the response
    beast::flat_buffer buff;
    http::response<http::string_body> res;
    co_await http::async_read(sock, buff, res, asio::deferred);
    std::cout << res << std::endl;
}

void handle_request(asio::any_io_executor ex)
{
    asio::co_spawn(ex, handle_request_impl, [](std::exception_ptr exc) {
        if (exc)
            std::rethrow_exception(exc);
    });
}

int main()
{
    asio::io_context ctx;
    handle_request(ctx.get_executor());
    ctx.run();
}