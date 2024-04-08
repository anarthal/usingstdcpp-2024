
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/bind_allocator.hpp>
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

#include <cstddef>
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

struct my_resource : public std::pmr::memory_resource
{
    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        std::cout << "Allocate " << bytes << " bytes\n";
        return std::pmr::get_default_resource()->allocate(bytes, alignment);
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override
    {
        std::pmr::get_default_resource()->deallocate(p, bytes, alignment);
    }

    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override
    {
        return dynamic_cast<const my_resource*>(&other);
    }
};

asio::awaitable<void> handle_request_impl()
{
    asio::any_io_executor ex = co_await asio::this_coro::executor;

    asio::ip::tcp::socket sock(ex);
    asio::ip::tcp::resolver resolv(ex);

    std::string buff;

    my_resource rsc;

    auto tok = asio::bind_allocator(std::pmr::polymorphic_allocator<void>(&rsc), asio::deferred);

    auto endpoints = co_await resolv.async_resolve("python.org", "80", tok);
    co_await asio::async_connect(sock, endpoints, tok);

    co_await asio::async_write(sock, asio::buffer(request), tok);
    std::size_t
        bytes_read = co_await asio::async_read_until(sock, asio::dynamic_buffer(buff), "\r\n\r\n", tok);

    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

void handle_request(asio::any_io_executor ex)
{
    asio::co_spawn(ex, handle_request_impl, [](...) {});
}

int main()
{
    asio::io_context ctx;
    handle_request(ctx.get_executor());
    ctx.run();
}