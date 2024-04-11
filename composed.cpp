
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/io_context.hpp>
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

// The GET HTTP request to send to the server
static constexpr std::string_view request =
    "GET / HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "User-Agent: Asio\r\n"
    "Accept: */*\r\n\r\n";

// Bonus: this is how you write a generic composed operation
struct handle_request_op
{
    // Operation state
    asio::ip::tcp::resolver& resolv;
    asio::ip::tcp::socket& sock;
    std::string_view req;
    std::string& buff;

    enum class state_t
    {
        initial,
        resolving,
        connecting,
        writing,
        reading,
    } state{state_t::initial};

    // Called when the operation is initiated
    template <class Self>
    void operator()(Self& self)
    {
        assert(state == state_t::initial);
        state = state_t::resolving;
        resolv.async_resolve("example.com", "80", std::move(self));
    }

    // Called when async_resolve completes
    template <class Self>
    void operator()(Self& self, error_code ec, asio::ip::tcp::resolver::results_type endpoints)
    {
        if (ec)
            return self.complete(ec, 0u);
        assert(state == state_t::resolving);
        state = state_t::connecting;
        asio::async_connect(sock, endpoints, std::move(self));
    }

    // Called when async_connect completes
    template <class Self>
    void operator()(Self& self, error_code ec, asio::ip::tcp::endpoint)
    {
        if (ec)
            return self.complete(ec, 0u);
        assert(state == state_t::connecting);
        state = state_t::writing;
        asio::async_write(sock, asio::buffer(request), std::move(self));
    }

    // Called when async_write and async_write complete
    template <class Self>
    void operator()(Self& self, error_code ec, std::size_t bytes_transferred)
    {
        if (ec)
            return self.complete(ec, 0u);

        if (state == state_t::writing)
        {
            state = state_t::reading;
            asio::async_read_until(sock, asio::dynamic_buffer(buff), "\r\n\r\n", std::move(self));
        }
        else
        {
            assert(state == state_t::reading);
            self.complete(error_code(), bytes_transferred);
        }
    }
};

template <asio::completion_token_for<void(error_code, std::size_t)> CompletionToken>
auto handle_request_generic(
    asio::ip::tcp::resolver& resolv,
    asio::ip::tcp::socket& sock,
    std::string_view req,
    std::string& buff,
    CompletionToken&& token
)
{
    // Takes care of the completion token return type transformation,
    // propagates the associated characteristics...
    return asio::async_compose<CompletionToken, void(error_code, std::size_t)>(
        handle_request_op{resolv, sock, req, buff},
        token,
        resolv,
        sock
    );
}

asio::awaitable<void> handle_request_impl()
{
    // Get the executor
    asio::any_io_executor ex = co_await asio::this_coro::executor;

    // These variables need to remain valid until the operation completes
    asio::ip::tcp::socket sock(ex);
    asio::ip::tcp::resolver resolv(ex);
    std::string buff;

    // Call our operation
    std::size_t bytes_read = co_await handle_request_generic(resolv, sock, request, buff, asio::deferred);
    std::cout << std::string_view(buff.data(), bytes_read) << std::endl;
}

void handle_request(asio::any_io_executor ex)
{
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