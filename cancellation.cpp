
#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

#include <chrono>
#include <iostream>

namespace asio = boost::asio;
using boost::system::error_code;

int main()
{
    // Execution context
    asio::io_context ctx;

    // I/O objects
    asio::ip::tcp::socket sock{ctx};
    asio::steady_timer timer{ctx};

    // Endpoint to connect to
    const auto endpoint = asio::ip::tcp::endpoint(asio::ip::address::from_string("80.213.10.1"), 80);

    // An object that can emit a signal to cause per-operation cancellation.
    asio::cancellation_signal sig;

    // Wait 5 seconds, then trigger cancellation
    timer.expires_after(std::chrono::seconds(5));
    timer.async_wait([&sig](error_code) {
        // When the timer fires, trigger cancellation
        sig.emit(asio::cancellation_type::terminal);
    });

    // Start an operation, binding our completion token to the signal's slot.
    // When the signal is emitted, the operation is cancelled
    sock.async_connect(endpoint, asio::bind_cancellation_slot(sig.slot(), [](error_code ec) {
                           std::cout << "Connect finished: " << ec.message() << std::endl;
                       }));

    // Run the application
    ctx.run();
}