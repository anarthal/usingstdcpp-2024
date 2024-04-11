
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/bind_allocator.hpp>
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

// A custom allocator that logs allocation and deallocation requests
template <class T>
struct custom_allocator
{
    using value_type = T;

    custom_allocator() = default;

    template <class U>
    constexpr custom_allocator(const custom_allocator<U>&) noexcept
    {
    }

    T* allocate(std::size_t n)
    {
        std::cout << "Allocate " << n * sizeof(T) << " bytes\n";
        return std::allocator<T>().allocate(n);
    }
    void deallocate(T* p, std::size_t n)
    {
        std::cout << "Deallocate " << n * sizeof(T) << "bytes\n";
        return std::allocator<T>().deallocate(p, n);
    }
};

asio::awaitable<void> handle_request_impl()
{
    // Coroutines know which executor are using
    asio::any_io_executor ex = co_await asio::this_coro::executor;

    // I/O objects
    asio::ip::tcp::socket sock(ex);
    asio::ip::tcp::resolver resolv(ex);

    // A completion token with an associated allocator
    auto tok = asio::bind_allocator(custom_allocator<void>(), asio::deferred);

    // Resolve the hostname and port into a set of endpoints
    auto endpoints = co_await resolv.async_resolve("example.com", "80", tok);

    // Connect to the server
    co_await asio::async_connect(sock, endpoints, tok);

    // Write the request
    co_await asio::async_write(sock, asio::buffer(request), tok);

    // Read the response
    std::string buff;
    std::size_t
        bytes_read = co_await asio::async_read_until(sock, asio::dynamic_buffer(buff), "\r\n\r\n", tok);
    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

void handle_request(asio::any_io_executor ex)
{
    // Rethrow exceptions originated in the coroutine
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