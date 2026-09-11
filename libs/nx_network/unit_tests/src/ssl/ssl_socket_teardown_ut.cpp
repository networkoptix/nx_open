// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <chrono>
#include <cstdlib>
#include <expected>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include <nx/utils/json/flags.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/test_support/utils.h>
#include <nx/utils/thread/cf/cfuture.h>

#include "ssl_socket_ut.h"

namespace nx::network::ssl::test {

namespace {

using namespace std::chrono;

/** Returns a bound and listening socket. Its endpoint is getLocalAddress(). */
std::expected<std::unique_ptr<EnforcedSslOverTcpServerSocket>, std::string> listenOnAnyPort()
{
    std::unique_ptr socket = std::make_unique<EnforcedSslOverTcpServerSocket>();
    if (!socket->setNonBlockingMode(true))
        return std::unexpected("Failed to switch the server socket to non-blocking mode");
    if (!socket->bind(SocketAddress::anyPrivateAddress))
        return std::unexpected("Failed to bind: " + SystemError::getLastOSErrorText());
    if (!socket->listen())
        return std::unexpected("Failed to listen: " + SystemError::getLastOSErrorText());
    if (!socket->getLocalAddress().port)
        return std::unexpected("The server socket was not assigned a port");
    return socket;
}

/** The connection stays alive inside the returned future until that future is destroyed. */
cf::future<std::unique_ptr<AbstractStreamSocket>> acceptNextConnection(
    EnforcedSslOverTcpServerSocket* socket)
{
    cf::promise<std::unique_ptr<AbstractStreamSocket>> promise;
    cf::future accepted = promise.get_future();
    socket->acceptAsync(
        [promise = std::move(promise)](
            SystemError::ErrorCode code, std::unique_ptr<AbstractStreamSocket> connection) mutable
        {
            EXPECT_EQ(SystemError::noError, code) << SystemError::toString(code);
            promise.set_value(std::move(connection));
        });
    return accepted;
}

std::expected<std::unique_ptr<SslOverTcpStreamSocket>, std::string> connectClient(
    const SocketAddress& endpoint)
{
    std::unique_ptr socket = std::make_unique<SslOverTcpStreamSocket>();
    if (!socket->connect(endpoint, 7s))
        return std::unexpected("Failed to connect: " + SystemError::getLastOSErrorText());
    if (!socket->setNonBlockingMode(true))
        return std::unexpected("Failed to switch the client socket to non-blocking mode");
    return socket;
}

/**
 * Never settles against a silent peer, so is_ready() reports whether the read is still queued.
 * The buffer is owned by the operation, so it lives exactly as long as the read does.
 */
cf::future<nx::Buffer> readAsync(SslOverTcpStreamSocket* socket)
{
    cf::promise<nx::Buffer> promise;
    cf::future read = promise.get_future();

    std::unique_ptr buffer = std::make_unique<nx::Buffer>();
    buffer->reserve(4096);
    auto* const target = buffer.get();

    socket->readSomeAsync(target,
        [buffer = std::move(buffer), promise = std::move(promise)](
            SystemError::ErrorCode code, std::size_t /*bytesRead*/) mutable
        {
            // A failed read still settles the future, which is all the caller samples, so the
            // expectation is what keeps a broken connection from looking like a completed read.
            EXPECT_EQ(SystemError::noError, code) << SystemError::toString(code);
            promise.set_value(std::move(*buffer));
        });
    return read;
}

/**
 * Destroying an ssl::StreamSocket tears down, in this order, the SSL pipeline and the
 * transforming channel that reads through it. A read left pending at that moment is what makes
 * the order observable: the channel is still processing it while the pipeline is already gone.
 */
NX_REFLECTION_ENUM_CLASS(Teardown,
    withPendingRead = 1 << 0, //< Issue a readSomeAsync the peer never satisfies before destroying.
    inAioThread = 1 << 1 //< Destroy from the socket's AIO thread rather than from the caller's.
)

Q_DECLARE_FLAGS(TeardownFlags, Teardown)
Q_DECLARE_OPERATORS_FOR_FLAGS(TeardownFlags)

/** Destroys the socket, from its own AIO thread when the options ask for it. */
std::expected<void, std::string> destroySocket(
    std::unique_ptr<SslOverTcpStreamSocket> socket, TeardownFlags options)
{
    if (!(options & Teardown::inAioThread))
    {
        socket.reset();
        return {};
    }

    cf::promise<cf::unit> promise;
    cf::future destroyed = promise.get_future();
    auto* const aioThreadOwner = socket.get();
    aioThreadOwner->post(
        [socket = std::move(socket), promise = std::move(promise)]() mutable
        {
            socket.reset();
            promise.set_value(cf::unit());
        });

    if (cf::future_status::ready != destroyed.wait_for(30s))
        return std::unexpected("Timed out waiting for the in-AIO-thread teardown");
    return {};
}

class SslSocketTeardown: public ::testing::TestWithParam<TeardownFlags>
{
};

/**
 * Starts a read the peer never answers, then destroys the socket while that read is still queued.
 * Destroying a socket with I/O in flight is expected to work: the read is cancelled together
 * with the socket, and nothing of it runs once the socket is gone.
 *
 * Each flag combination destroys from a different thread, with and without the pending read.
 */
TEST_P(SslSocketTeardown, pending_read_does_not_outlive_the_ssl_pipeline)
{
    const TeardownFlags options = GetParam();

    // EXPECT_EXIT below runs the scenario in a separate process and judges it by that process's
    // exit code, so the failure code must not be one the process could produce by dying: a crash
    // reaching abort() exits with 3.
    constexpr int kSucceeded = 0;
    constexpr int kFailed = 61;

    // nx::utils::test::runTest already forces the threadsafe death test style.
    EXPECT_EXIT(
        {
            // One attempt does not fail reliably on a broken build - it missed once in five
            // trial runs, since a read left behind is only noticed when the memory it touches
            // has been reused. Repeating makes the failure dependable.
            constexpr int kAttempts = 20;

            const std::expected server = listenOnAnyPort();
            if (!server)
            {
                ADD_FAILURE() << server.error();
                std::_Exit(kFailed);
            }

            int readsPendingAtTeardown = 0;
            for (const int attempt: std::views::iota(0, kAttempts))
            {
                SCOPED_TRACE(::testing::Message() << "attempt " << attempt);

                // Armed before connecting, and held for the whole attempt: dropping the peer
                // would complete the client read.
                cf::future accepted = acceptNextConnection(server->get());

                // Not const: destroySocket below takes the socket over.
                std::expected client = connectClient(server->get()->getLocalAddress());
                if (!client.has_value())
                {
                    ADD_FAILURE() << client.error();
                    break;
                }

                // Sampled before destroying: afterwards the abandoned promise reports ready
                // regardless of whether the read ever completed.
                const bool readIsQueued =
                    (options & Teardown::withPendingRead) && !readAsync(client->get()).is_ready();

                const std::expected destroyed = destroySocket(std::move(*client), options);
                if (!destroyed.has_value())
                {
                    ADD_FAILURE() << destroyed.error();
                    break;
                }

                readsPendingAtTeardown += readIsQueued ? 1 : 0;

                if (cf::future_status::ready != accepted.wait_for(30s))
                {
                    ADD_FAILURE() << "Timed out waiting for the peer to be accepted";
                    break;
                }
            }

            // Fewer than every attempt means some of them missed the window and prove nothing.
            EXPECT_EQ(
                (options & Teardown::withPendingRead) ? kAttempts : 0, readsPendingAtTeardown);

            // A gtest failure recorded here reaches the test only through the exit code.
            // _Exit leaves SocketGlobals deinit unrun on purpose, so teardown-time checks cannot
            // perturb the code being matched on.
            std::_Exit(::testing::Test::HasFailure() ? kFailed : kSucceeded);
        },
        ::testing::ExitedWithCode(kSucceeded),
        ::testing::_);
}

const std::vector<TeardownFlags> kScenarios = {
    {},
    Teardown::withPendingRead,
    Teardown::inAioThread,
    Teardown::withPendingRead | Teardown::inAioThread,
};

INSTANTIATE_TEST_SUITE_P(Ssl,
    SslSocketTeardown,
    ::testing::ValuesIn(kScenarios),
    /*nameGenerator*/
    [](const auto& info)
    {
        // toString() of empty flags is an empty string, which gtest will not accept as a name.
        const std::string name = toString(info.param);
        return nx::utils::test::normalizedGTestNameString(name.empty() ? "none" : name);
    });

} // namespace

} // namespace nx::network::ssl::test
