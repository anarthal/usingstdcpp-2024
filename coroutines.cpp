
#include <boost/asio/any_io_executor.hpp>
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

#include <cstddef>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace asio = boost::asio;

// The GET HTTP request to send to the server
static constexpr std::string_view request =
    "GET / HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "User-Agent: Asio\r\n"
    "Accept: */*\r\n\r\n";

// This function is a coroutine, since it returns asio::awaitable
// and uses co_await in its body.
// asio::deferred is a completion token that packs an async operation
// into an object that can be co_await'ed.
// A completion token may thus change when an operation is initiated
// and its return type.
asio::awaitable<void> handle_request_impl()
{
    // Coroutines know which executor are using
    asio::any_io_executor ex = co_await asio::this_coro::executor;

    // I/O objects
    asio::ip::tcp::socket sock(ex);
    asio::ip::tcp::resolver resolv(ex);

    // Resolve the hostname and port into a set of endpoints
    auto endpoints = co_await resolv.async_resolve("example.com", "80", asio::deferred);

    // Connect to the server
    co_await asio::async_connect(sock, endpoints, asio::deferred);

    // Write the request
    auto a = asio::async_write(sock, asio::buffer(request), asio::deferred);
    co_await asio::async_write(sock, asio::buffer(request), asio::deferred);

    // Read the response
    std::string buff;
    std::size_t bytes_read = co_await asio::async_read_until(
        sock,
        asio::dynamic_buffer(buff),
        "\r\n\r\n",
        asio::deferred
    );
    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

void handle_request(asio::any_io_executor ex)
{
    // co_spawn is an initiating function that starts a coroutine.
    // If an exception is thrown in the coroutine, exc will be non-null.
    // Rethrowing it propagates it up, making run() throw.
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
