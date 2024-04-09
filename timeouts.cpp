
#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/experimental/cancellation_condition.hpp>
#include <boost/asio/experimental/co_composed.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/system/error_code.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>

namespace asio = boost::asio;

static constexpr std::string_view request =
    "GET / HTTP/1.1\r\n"
    "Host: www.python.org\r\n"
    "User-Agent: curl/7.71.1\r\n"
    "Accept: */*\r\n\r\n";

asio::awaitable<void> handle_request_impl()
{
    asio::any_io_executor ex = co_await asio::this_coro::executor;

    asio::ip::tcp::socket sock(ex);
    asio::ip::tcp::resolver resolv(ex);
    asio::steady_timer tim(ex);
    tim.expires_after(std::chrono::seconds(1));
    co_await tim.async_wait(asio::as_tuple(asio::deferred));

    std::string buff;

    auto endpoints = co_await resolv.async_resolve("python.org", "80", asio::deferred);
    co_await asio::async_connect(sock, endpoints, asio::deferred);
    co_await asio::async_write(sock, asio::buffer(request), asio::deferred);
    std::size_t bytes_read = co_await asio::async_read_until(
        sock,
        asio::dynamic_buffer(buff),
        "\r\n\r\n",
        asio::deferred
    );

    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

asio::awaitable<void> handle_request_with_timeout()
{
    auto ex = co_await asio::this_coro::executor;

    asio::steady_timer timer(ex);
    timer.expires_after(std::chrono::seconds(5));

    // clang-format off
    auto [completion_order, exc, ec2] = co_await asio::experimental::make_parallel_group(
        asio::co_spawn(ex, handle_request_impl, asio::deferred),
        timer.async_wait(asio::deferred)
    ).async_wait(
        asio::experimental::wait_for_one(),
        asio::deferred
    );
    // clang-format on

    if (exc)
        std::rethrow_exception(exc);
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