// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <future>
#include <thread>

#include <gtest/gtest.h>

#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>
#include <QtNetwork/QHostAddress>

#include <network/tcp_connection_priv.h>
#include <network/tcp_connection_processor.h>
#include <network/tcp_listener.h>
#include <nx/network/socket_common.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/system_error.h>
#include <nx/utils/test_support/sync_queue.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/thread/long_runnable.h>
#include <nx/vms/common/test_support/test_context.h>
#include <recording/time_period_list.h>

namespace {
    static const int kDataTransferTimeout = 1000;
    static const int kTotalTestBytes = 1024 * 1024 * 20;
    static const int kTcpServerStartTimeoutMs = 1000;

    /** Corresponds to the same-name Server setting. */
    static const int kMaxTcpRequestSize = 16 * 1024 * 1024;
}

class TestConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    TestConnectionProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner,
        int maxTcpRequestSize)
        :
        QnTCPConnectionProcessor(std::move(socket), owner, maxTcpRequestSize)
    {
    }
    virtual ~TestConnectionProcessor() override
    {
        stop();
    }

protected:
    virtual void run() override
    {
        Q_D(QnTCPConnectionProcessor);
        ASSERT_TRUE(d->socket->setNonBlockingMode(true));
        ASSERT_TRUE(d->socket->setSendTimeout(kDataTransferTimeout));

        static const int kSteps = 64;
        std::vector<char> buffer(kTotalTestBytes / kSteps);
        for (int i = 0; i < kSteps; ++i)
            ASSERT_TRUE(sendBuffer(buffer.data(), (int) buffer.size()));
    }

};

class TestTcpListener: public QnTcpListener
{
public:
    TestTcpListener(
        nx::vms::common::SystemContext* systemContext,
        int maxTcpRequestSize,
        const QHostAddress& address,
        int port)
        :
        QnTcpListener(
            systemContext,
            maxTcpRequestSize,
            address,
            port,
            DEFAULT_MAX_CONNECTIONS,
            /*useSSL*/ false)
    {
    }

    virtual ~TestTcpListener() override
    {
        stop();
    }

protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> clientSocket,
        int maxTcpRequestSize) override
    {
        return new TestConnectionProcessor(std::move(clientSocket), this, maxTcpRequestSize);
    }
};


TEST(TcpConnectionProcessor, sendAsyncData)
{
    nx::vms::common::test::Context context;

    // QnTcpListener uses commonModule()->moduleGuid().
    TestTcpListener tcpListener(
        context.systemContext(), kMaxTcpRequestSize, QHostAddress::Any, /*port*/ 0);
    tcpListener.start();

    QElapsedTimer timer;
    timer.restart();
    while (!timer.hasExpired(kTcpServerStartTimeoutMs) && tcpListener.getPort() == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto clientSocket = nx::network::SocketFactory::createStreamSocket(
        nx::network::ssl::kAcceptAnyCertificate, /*sslRequired*/ false);
    ASSERT_TRUE(clientSocket->setRecvTimeout(kDataTransferTimeout));
    ASSERT_TRUE(clientSocket->connect(
        nx::network::SocketAddress(nx::network::HostAddress::localhost, tcpListener.getPort()),
        nx::network::kNoTimeout));

    char buffer[kTotalTestBytes / 128];
    int gotBytes = 0;
    while (gotBytes < kTotalTestBytes)
    {
        int bytesRead = clientSocket->recv(buffer, sizeof(buffer));
        if (bytesRead < 0 && SystemError::getLastOSErrorCode() == SystemError::timedOut)
            continue;

        ASSERT_GT(bytesRead, 0) << SystemError::getLastOSErrorText();
        gotBytes += bytesRead;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

namespace {

// Finishes at once; its destructor blocks until released, like one whose socket is bound to a
// stuck AIO thread.
class BlockingDeleteListener: public QnTcpListener
{
public:
    enum class Event
    {
        started,
        deleting
    };
    nx::utils::TestSyncQueue<Event> events{std::chrono::seconds(5)};
    std::promise<void> release;
    const std::shared_future<void> released = release.get_future().share();

    BlockingDeleteListener(nx::vms::common::SystemContext* systemContext):
        QnTcpListener(systemContext,
            kMaxTcpRequestSize,
            QHostAddress::Any,
            /*port*/ 0,
            DEFAULT_MAX_CONNECTIONS,
            /*useSSL*/ false)
    {
    }

    virtual ~BlockingDeleteListener() override { stop(); }

protected:
    class Processor: public QnTCPConnectionProcessor
    {
    public:
        Processor(std::unique_ptr<nx::network::AbstractStreamSocket> socket,
            BlockingDeleteListener* owner):
            QnTCPConnectionProcessor(std::move(socket), owner, kMaxTcpRequestSize),
            m_owner(owner)
        {
        }

        virtual ~Processor() override
        {
            m_owner->events.push(Event::deleting);
            m_owner->released.wait();
            stop();
        }

    protected:
        virtual void run() override { m_owner->events.push(Event::started); }

    private:
        BlockingDeleteListener* const m_owner;
    };

    virtual QnTCPConnectionProcessor* createRequestProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> clientSocket,
        int /*maxTcpRequestSize*/) override
    {
        return new Processor(std::move(clientSocket), this);
    }
};

std::unique_ptr<nx::network::AbstractStreamSocket> connectTo(const QnTcpListener& listener)
{
    auto socket = nx::network::SocketFactory::createStreamSocket(
        nx::network::ssl::kAcceptAnyCertificate, /*sslRequired*/ false);
    EXPECT_TRUE(socket->connect(
        nx::network::SocketAddress(nx::network::HostAddress::localhost, listener.getPort()),
        nx::network::kNoTimeout));
    return socket;
}

TEST(TcpListener, acceptsWhileProcessorDeletionIsBlocked)
{
    nx::vms::common::test::Context context;
    BlockingDeleteListener listener(context.systemContext());
    const auto releaseGuard = nx::utils::makeScopeGuard([&]() { listener.release.set_value(); });
    ASSERT_TRUE(listener.bindToLocalAddress());
    listener.start();

    using Event = BlockingDeleteListener::Event;
    const auto first = connectTo(listener);
    ASSERT_EQ(listener.events.pop(), Event::started);
    ASSERT_EQ(listener.events.pop(), Event::deleting);

    const auto second = connectTo(listener);
    EXPECT_EQ(listener.events.pop(), Event::started);
}

// stopAll() stops the listener's deleter worker too, possibly first.
TEST(TcpListener, poolStopAllDeletesFinishedProcessors)
{
    nx::vms::common::test::Context context;
    QnLongRunnablePool pool;
    BlockingDeleteListener listener(context.systemContext());
    listener.release.set_value();
    ASSERT_TRUE(listener.bindToLocalAddress());
    listener.start();

    const auto socket = connectTo(listener);
    ASSERT_EQ(listener.events.pop(), BlockingDeleteListener::Event::started);

    pool.stopAll();
    EXPECT_EQ(listener.events.pop(), BlockingDeleteListener::Event::deleting);
}

// A stopped Worker drops posted tasks, so a listener running after stopAll() deletes inline.
TEST(TcpListener, deletesFinishedProcessorsAfterDeleterIsStopped)
{
    nx::vms::common::test::Context context;
    QnLongRunnablePool pool;
    BlockingDeleteListener listener(context.systemContext());
    listener.release.set_value();
    pool.stopAll();
    ASSERT_TRUE(listener.bindToLocalAddress());
    listener.start();

    const auto socket = connectTo(listener);
    ASSERT_EQ(listener.events.pop(), BlockingDeleteListener::Event::started);
    EXPECT_EQ(listener.events.pop(), BlockingDeleteListener::Event::deleting);
}

} // namespace
