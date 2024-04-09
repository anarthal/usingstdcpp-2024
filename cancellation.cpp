
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/bind_allocator.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detail/handler_work.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/detail/error_code.hpp>

#include <chrono>
#include <iostream>

namespace asio = boost::asio;
using boost::system::error_code;

int main()
{
    asio::io_context ctx;
    asio::ip::tcp::socket sock{ctx};
    asio::steady_timer timer{ctx};

    asio::cancellation_signal sig;

    sock.async_connect(
        asio::ip::tcp::endpoint(asio::ip::address::from_string("80.213.10.1"), 80),
        asio::bind_cancellation_slot(
            sig.slot(),
            [](error_code ec) { std::cout << "Connect finished: " << ec.message() << std::endl; }
        )
    );

    timer.expires_after(std::chrono::milliseconds(10000));
    timer.async_wait([&sig](error_code) { sig.emit(asio::cancellation_type::terminal); });
    ctx.run();
}