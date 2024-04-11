
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <cstddef>
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

asio::awaitable<void> handle_request_impl()
{
    // Coroutines know which executor are using
    asio::any_io_executor ex = co_await asio::this_coro::executor;

    // I/O objects
    asio::ip::tcp::socket sock(ex);
    asio::ip::tcp::resolver resolv(ex);
    asio::steady_timer timer(ex);

    // Resolve the hostname and port into a set of endpoints
    auto endpoints = co_await resolv.async_resolve("example.com", "80", asio::deferred);

    // Connect to the server
    co_await asio::async_connect(sock, endpoints, asio::deferred);

    // Simulate a delay
    timer.expires_after(std::chrono::seconds(8));
    co_await timer.async_wait(asio::deferred);

    // Write the request
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

int main()
{
    // Execution context
    asio::io_context ctx;

    // I/O objects
    asio::ip::tcp::socket sock{ctx};
    asio::steady_timer timer{ctx};

    // An object that can emit a signal to cause per-operation cancellation.
    asio::cancellation_signal sig;

    // Wait 5 seconds, then trigger cancellation
    timer.expires_after(std::chrono::seconds(5));
    timer.async_wait([&sig](error_code) {
        // When the timer fires, trigger cancellation
        sig.emit(asio::cancellation_type::terminal);
    });

    // co_spawn is a regular initiating function, so we can use the same cancellation mechanism.
    // The slot is propagated to the coroutine, and to individual functions by co_await.
    // Note that proxy cancellation_signal's and slots are created in the process
    asio::co_spawn(
        ctx,
        handle_request_impl,
        asio::bind_cancellation_slot(
            sig.slot(),
            [](std::exception_ptr ptr) {
                if (ptr)
                    std::rethrow_exception(ptr);
            }
        )
    );

    // Run the application
    ctx.run();
}