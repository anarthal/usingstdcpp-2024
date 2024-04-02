
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

static constexpr std::string_view request =
    "GET / HTTP/1.1\r\n"
    "Host: www.python.org\r\n"
    "User-Agent: curl/7.71.1\r\n"
    "Accept: */*\r\n\r\n";

class request_handler : public std::enable_shared_from_this<request_handler>
{
    asio::ip::tcp::socket sock;
    asio::ip::tcp::resolver resolv;
    std::string buff;

public:
    request_handler(asio::any_io_executor ex) : sock(ex), resolv(ex) {}

    void start_resolve()
    {
        resolv.async_resolve(
            "python.org",
            "80",
            [self = shared_from_this()](error_code ec, asio::ip::tcp::resolver::results_type endpoints) {
                if (ec)
                    std::cerr << "Error resolving endpoints: " << ec.message() << std::endl;
                else
                    self->start_connect(std::move(endpoints));
            }
        );
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

int main()
{
    asio::io_context ctx;
    std::make_shared<request_handler>(ctx.get_executor())->start_resolve();
    ctx.run();
}