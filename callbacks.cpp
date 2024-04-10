
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <iostream>
#include <memory>
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

// Create one of these per handle_request call
class request_handler : public std::enable_shared_from_this<request_handler>
{
    // All these variables need to be kept alive until the overall handle_request operation completes
    // (they need to have stable addresses)
    asio::ip::tcp::socket sock;
    asio::ip::tcp::resolver resolv;
    std::string buff;

public:
    request_handler(asio::any_io_executor ex) : sock(ex), resolv(ex) {}

    // Start the async chain. We capture this as a shared_ptr to ensure correct lifetimes
    void start_resolve()
    {
        resolv.async_resolve("example.com", "80", [self = shared_from_this()](error_code ec, auto endpoints) {
            if (ec)
                std::cerr << "Error resolving endpoints: " << ec.message() << std::endl;
            else
                self->start_connect(std::move(endpoints));
        });
    }

    void start_connect(const asio::ip::tcp::resolver::results_type& endpoints)
    {
        asio::async_connect(sock, endpoints, [self = shared_from_this()](error_code ec, auto) {
            if (ec)
                std::cerr << "Error connecting: " << ec.message() << std::endl;
            else
                self->start_write();
        });
    }

    void start_write()
    {
        asio::async_write(
            sock,
            asio::buffer(request),
            [self = shared_from_this()](error_code ec, std::size_t bytes_written) {
                if (ec)
                    std::cerr << "Error writing: " << ec.message() << std::endl;
                else
                    self->start_read();
            }
        );
    }

    // This is the end of the async chain
    void start_read()
    {
        asio::async_read_until(
            sock,
            asio::dynamic_buffer(buff),
            "\r\n\r\n",
            [self = shared_from_this()](error_code ec, std::size_t bytes_read) {
                if (ec)
                    std::cerr << "Error reading: " << ec.message() << std::endl;
                else
                    std::cout << std::string_view(self->buff.data(), bytes_read) << std::endl;
            }
        );
    }
};

void handle_request(asio::any_io_executor ex)
{
    // Create a request_handler and start the chain
    std::make_shared<request_handler>(ex)->start_resolve();
}

int main()
{
    // The execution context
    asio::io_context ctx;

    // Call out function
    handle_request(ctx.get_executor());

    // Run the event loop until the chain finishes
    ctx.run();
}