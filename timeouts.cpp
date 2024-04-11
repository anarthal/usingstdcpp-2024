
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/experimental/cancellation_condition.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/write.hpp>

#include <chrono>
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

// Another coroutine wrapping handle_request_impl in a parallel_group,
// to set a timeout to the overall operation
asio::awaitable<void> handle_request_with_timeout()
{
    // Coroutines know which executor are using
    auto ex = co_await asio::this_coro::executor;

    // Setup a timer
    asio::steady_timer timer(ex);
    timer.expires_after(std::chrono::seconds(5));

    // Launch the coroutine and the timer in parallel.
    // When one of the operations finishes, the other will be cancelled
    // using per-operation cancellation.
    // completion_order is a std::array<std::size_t, 2> containing the order in which operations completed
    // clang-format off
    auto [completion_order, coro_exc, timer_ec] = co_await asio::experimental::make_parallel_group(
        asio::co_spawn(ex, handle_request_impl, asio::deferred),
        timer.async_wait(asio::deferred)
    ).async_wait(
        asio::experimental::wait_for_one(),
        asio::deferred
    );
    // clang-format on

    // Check for errors
    if (coro_exc)
        std::rethrow_exception(coro_exc);
}

void handle_request(asio::any_io_executor ex)
{
    asio::co_spawn(ex, handle_request_with_timeout, [](std::exception_ptr exc) {
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