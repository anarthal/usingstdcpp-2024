
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>

namespace asio = boost::asio;
using boost::system::error_code;

static constexpr std::string_view request =
    "GET / HTTP/1.1\r\n"
    "Host: www.python.org\r\n"
    "User-Agent: curl/7.71.1\r\n"
    "Accept: */*\r\n\r\n";

void read_headers_v1(
    asio::ip::tcp::socket& sock,
    std::string_view req,
    std::string& buff,
    std::function<void(error_code, std::size_t)> h
)
{
    asio::async_write(
        sock,
        asio::buffer(req),
        [&sock, &buff, h = std::move(h)](error_code ec, std::size_t) mutable {
            if (ec)
                h(ec, 0u);
            else
                asio::async_read_until(sock, asio::dynamic_buffer(buff), "\r\n\r\n", h);
        }
    );
}

struct read_headers_op
{
    asio::ip::tcp::socket& sock;
    std::string_view req;
    std::string& buff;

    enum class state_t
    {
        initial,
        writing,
        reading
    } state{state_t::initial};

    template <class Self>
    void operator()(Self& self, error_code ec = {}, std::size_t bytes_transferred = 0)
    {
        switch (state)
        {
        case state_t::initial:
            state = state_t::writing;
            asio::async_write(sock, asio::buffer(req), std::move(self));
            break;
        case state_t::writing:
            if (ec)
            {
                self.complete(ec, 0u);
            }
            else
            {
                state = state_t::reading;
                asio::async_read_until(sock, asio::dynamic_buffer(buff), "\r\n\r\n", std::move(self));
            }
            break;
        case state_t::reading: self.complete(ec, bytes_transferred); break;
        }
    }
};

template <asio::completion_token_for<void(error_code, std::size_t)> CompletionToken>
auto read_headers_v2(
    asio::ip::tcp::socket& sock,
    std::string_view req,
    std::string& buff,
    CompletionToken&& token
)
{
    return asio::async_compose<CompletionToken, void(error_code, std::size_t)>(
        read_headers_op{sock, req, buff},
        token,
        sock
    );
}

asio::awaitable<error_code> handle_request_impl()
{
    asio::any_io_executor ex = co_await asio::this_coro::executor;

    asio::ip::tcp::socket sock(ex);
    asio::ip::tcp::resolver resolv(ex);

    std::string buff;

    auto [ec1, endpoints] = co_await resolv.async_resolve("python.org", "80", asio::as_tuple(asio::deferred));
    if (ec1)
        co_return ec1;
    auto [ec2, ep] = co_await asio::async_connect(sock, endpoints, asio::as_tuple(asio::deferred));
    if (ec2)
        co_return ec2;

    auto [ec3, bytes_read] = co_await read_headers_v2(sock, request, buff, asio::as_tuple(asio::deferred));
    if (ec3)
        co_return ec3;

    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
    co_return error_code();
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