
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>

namespace asio = boost::asio;

static constexpr std::string_view request =
    "GET / HTTP/1.1\r\n"
    "Host: www.python.org\r\n"
    "User-Agent: curl/7.71.1\r\n"
    "Accept: */*\r\n\r\n";

void handle_request_v0(asio::io_context& ctx)
{
    asio::ip::tcp::socket sock(ctx);

    std::string buff(4096, '\0');

    sock.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("151.101.64.223"), 80));
    sock.write_some(asio::buffer(request));
    std::size_t bytes_read = sock.read_some(asio::buffer(buff));

    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

void handle_request_v1(asio::io_context& ctx)
{
    asio::ip::tcp::socket sock(ctx);

    std::string buff;

    sock.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("151.101.64.223"), 80));

    asio::write(sock, asio::buffer(request));
    std::size_t bytes_read = asio::read_until(sock, asio::dynamic_buffer(buff), "\r\n\r\n");

    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

void handle_request_v2(asio::io_context& ctx)
{
    asio::ip::tcp::socket sock(ctx);
    asio::ip::tcp::resolver resolv(ctx);

    std::string buff;

    auto endpoints = resolv.resolve("python.org", "80");
    asio::connect(sock, endpoints);

    asio::write(sock, asio::buffer(request));
    std::size_t bytes_read = asio::read_until(sock, asio::dynamic_buffer(buff), "\r\n\r\n");

    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

void handle_request_v3(asio::any_io_executor ex)
{
    asio::ip::tcp::socket sock(ex);
    asio::ip::tcp::resolver resolv(ex);

    std::string buff;

    auto endpoints = resolv.resolve("python.org", "80");
    asio::connect(sock, endpoints);

    asio::write(sock, asio::buffer(request));
    std::size_t bytes_read = asio::read_until(sock, asio::dynamic_buffer(buff), "\r\n\r\n");

    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

// No way to set timeouts

int main()
{
    asio::io_context ctx;
    handle_request_v0(ctx);
    handle_request_v1(ctx);
    handle_request_v2(ctx);
    handle_request_v3(ctx.get_executor());
}