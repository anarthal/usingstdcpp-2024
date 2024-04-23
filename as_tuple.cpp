
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/error_code.hpp>

#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace asio = boost::asio;
using boost::system::error_code;

// The GET HTTP request to send to the server
static constexpr std::string_view request =
    "GET / HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "User-Agent: Asio\r\n"
    "Accept: */*\r\n\r\n";

asio::awaitable<error_code> handle_request_impl()
{
    asio::any_io_executor ex = co_await asio::this_coro::executor;
    asio::ip::tcp::socket sock(ex);
    asio::ip::tcp::resolver resolv(ex);

    // asio::as_tuple is a completion token adapter that packs completion
    // handler arguments into a std::tuple before passing them to the completion handler.
    // Doing does inhibits the built-in error_code to exception translation
    constexpr auto tok = asio::as_tuple(asio::deferred);

    // Resolve the hostname and port into a set of endpoints
    auto [ec1, endpoints] = co_await resolv.async_resolve("example.com", "80", tok);
    if (ec1)
        co_return ec1;

    // Connect to the server
    auto [ec2, unused] = co_await asio::async_connect(sock, endpoints, tok);
    if (ec2)
        co_return ec2;

    // Write the request
    auto [ec3, bytes_written] = co_await asio::async_write(sock, asio::buffer(request), tok);
    if (ec3)
        co_return ec3;

    // Read the response
    std::string buff;
    auto [ec4, bytes_read] = co_await asio::async_read_until(
        sock,
        asio::dynamic_buffer(buff),
        "\r\n\r\n",
        tok
    );
    if (ec4)
        co_return ec4;
    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

void handle_request(asio::any_io_executor ex)
{
    // co_spawn is an initiating function that starts a coroutine.
    // If an exception is thrown in the coroutine, exc will be non-null.
    // Rethrowing it propagates it up, making run() throw.
    asio::co_spawn(ex, handle_request_impl, [](std::exception_ptr exc, error_code ec) {
        if (exc)
            std::rethrow_exception(exc);
        if (ec)
            std::cerr << "Error handling request: " << ec.message() << std::endl;
    });
}

int main()
{
    asio::io_context ctx;
    handle_request(ctx.get_executor());
    ctx.run();
}
