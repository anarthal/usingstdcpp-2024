
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detail/handler_work.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/write.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>

#include <cstddef>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;

static constexpr std::string_view request =
    "GET / HTTP/1.1\r\n"
    "Host: www.python.org\r\n"
    "User-Agent: curl/7.71.1\r\n"
    "Accept: */*\r\n\r\n";

asio::awaitable<void> handle_request_impl()
{
    auto ex = co_await asio::this_coro::executor;

    asio::ip::tcp::socket sock(ex);
    asio::ip::tcp::resolver resolv(ex);

    auto endpoints = co_await resolv.async_resolve("python.org", "80", asio::deferred);

    // Make the connection on the IP address we get from a lookup
    co_await asio::async_connect(sock, endpoints, asio::deferred);

    // Set up an HTTP GET request message
    http::request<http::string_body> req{http::verb::get, "/", 11};
    req.set(http::field::host, "python.org");
    req.set(http::field::user_agent, "Beast");

    // Send the HTTP request to the remote host
    co_await http::async_write(sock, req, asio::deferred);

    // This buffer is used for reading and must be persisted
    beast::flat_buffer b;

    // Declare a container to hold the response
    http::response<http::dynamic_body> res;

    // Receive the HTTP response
    co_await http::async_read(sock, b, res, asio::deferred);

    // Write the message to standard out
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