
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#include <array>
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

// v1: we connect to the server, attempt to write the request and read the response
void handle_request_v1(asio::io_context& ctx)
{
    asio::ip::tcp::socket sock(ctx);

    // Connect to the server
    sock.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("18.154.41.87"), 80));

    // Write the request
    sock.write_some(asio::buffer(request));

    // Read the response
    std::array<char, 1024> buff;
    std::size_t bytes_read = sock.read_some(asio::buffer(buff));
    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

// v2: the previous version may suffer from short reads and short writes.
// Use the composed asio::read and asio::write to avoid them
void handle_request_v2(asio::io_context& ctx)
{
    asio::ip::tcp::socket sock(ctx);

    // Connect to the server
    sock.connect(asio::ip::tcp::endpoint(asio::ip::address::from_string("18.154.41.87"), 80));

    // Write the request
    asio::write(sock, asio::buffer(request));

    // Read the response
    std::string buff;
    std::size_t bytes_read = asio::read_until(sock, asio::dynamic_buffer(buff), "\r\n\r\n");
    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

// v3: writing IPs manually is not practical. Use a resolver
// to convert a hostname and port into a set of endpoints we can connect to.
// The resolver may return more than one endpoint - e.g. IPv4 and IPv6
// asio::connect attempts to connect to each of these endpoints, until one succeeds
void handle_request_v3(asio::io_context& ctx)
{
    // I/O objects
    asio::ip::tcp::socket sock(ctx);
    asio::ip::tcp::resolver resolv(ctx);

    // Resolve the hostname and port into a set of endpoints
    auto endpoints = resolv.resolve("example.com", "80");

    // Connect to the server
    asio::connect(sock, endpoints);

    // Write the request
    asio::write(sock, asio::buffer(request));

    // Read the response
    std::string buff;
    std::size_t bytes_read = asio::read_until(sock, asio::dynamic_buffer(buff), "\r\n\r\n");
    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

// v4: by convention, we prefer to use executor instead of references to execution contexts
// Executor are lightweight handles to execution contexts. Using this is more generic.
void handle_request_v4(asio::any_io_executor ex)
{
    // I/O objects
    asio::ip::tcp::socket sock(ex);
    asio::ip::tcp::resolver resolv(ex);

    // Resolve the hostname and port into a set of endpoints
    auto endpoints = resolv.resolve("example.com", "80");

    // Connect to the server
    asio::connect(sock, endpoints);

    // Write the request
    asio::write(sock, asio::buffer(request));

    // Read the response
    std::string buff;
    std::size_t bytes_read = asio::read_until(sock, asio::dynamic_buffer(buff), "\r\n\r\n");
    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

// Deficiencies:
//   - No way to handle timeouts - can be solved by going async
//   - This doesn't read the body - can be solved using a higher-level library like Boost.Beast
//   - This is only valid for HTTP (not HTTPS) endpoints. Consult the Beast examples if you want to know more
//   - This doesn't handle redirection. This has to be implemented manually, although Boost.Beast helps

int main()
{
    // Create the execution context. You usually create one of these per application or per thread
    // (and never one per request)
    asio::io_context ctx;

    handle_request_v1(ctx);
    handle_request_v2(ctx);
    handle_request_v3(ctx);
    handle_request_v4(ctx.get_executor());
}