#include <chrono>
#include <memory>
#include <queue>

#include <gtest/gtest.h>

#include <QtCore/QElapsedTimer>

#include <nx/network/buffered_stream_socket.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/std/thread.h>

namespace nx {
namespace network {
namespace http {

static const nx::Buffer kPlayMessage("PLAY\r\n");
static const nx::Buffer kTeardownMessage("TEARDOWN\r\n");

class ResumableThread
{
public:
    ResumableThread(nx::utils::MoveOnlyFunc<void()> func):
        m_func(std::move(func))
    {
        m_thread = std::make_unique<nx::utils::thread>([this]() { threadFunc(); });
    }

    void join()
    {
        m_thread->join();
    }

    void resume()
    {
        m_resumed.set_value();
    }

private:
    nx::utils::MoveOnlyFunc<void()> m_func;
    std::unique_ptr<nx::utils::thread> m_thread;
    nx::utils::promise<void> m_resumed;

    void threadFunc()
    {
        m_resumed.get_future().wait();
        m_func();
    }
};

class ThreadStorage
{
public:
    ~ThreadStorage()
    {
        clear();
    }

    void add(std::unique_ptr<ResumableThread> thread)
    {
        QnMutexLocker lock(&m_mutex);
        m_threads.emplace_back(std::move(thread));
    }

    ResumableThread& back()
    {
        QnMutexLocker lock(&m_mutex);
        return *m_threads.back();
    }

    std::size_t size() const
    {
        QnMutexLocker lock(&m_mutex);
        return m_threads.size();
    }

    bool empty() const
    {
        QnMutexLocker lock(&m_mutex);
        return m_threads.empty();
    }

    void clear()
    {
        decltype(m_threads) threads;
        {
            QnMutexLocker lock(&m_mutex);
            threads.swap(m_threads);
        }

        for (auto& thread: threads)
            thread->join();
    }

private:
    mutable QnMutex m_mutex;
    std::vector<std::unique_ptr<ResumableThread>> m_threads;
};

class TakingSocketRestHandler:
    public nx::network::http::AbstractHttpRequestHandler
{
public:
    TakingSocketRestHandler(ThreadStorage* threadStorage):
        m_threadStorage(threadStorage)
    {
    }

    virtual void processRequest(
        nx::network::http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx::network::http::Request /*request*/,
        nx::network::http::Response* const /*response*/,
        nx::network::http::RequestProcessedHandler completionHandler) override
    {
        nx::network::http::ConnectionEvents events;
        events.onResponseHasBeenSent =
            [threadStorage = m_threadStorage](nx::network::http::HttpServerConnection* connection)
            {
                threadStorage->add(std::make_unique<ResumableThread>(
                    [sock = connection->takeSocket()]() mutable
                    {
                        ASSERT_TRUE(sock->setNonBlockingMode(false));
                        ASSERT_GT(sock->send(kPlayMessage), 0);
                        ASSERT_GT(sock->send(kTeardownMessage), 0);
                        sock.reset();
                    }));
            };

        completionHandler(
            nx::network::http::RequestResult(
                nx::network::http::StatusCode::ok,
                nullptr,
                std::move(events)));
    }

private:
    ThreadStorage* m_threadStorage;
};

class TakingHttpSocketTest:
    public ::testing::Test
{
protected:
    std::unique_ptr<TestHttpServer> testHttpServer()
    {
        return std::make_unique<TestHttpServer>();
    }

    std::unique_ptr<nx::network::BufferedStreamSocket> takeSocketFromHttpClient(
        std::unique_ptr<nx::network::http::HttpClient>& httpClient)
    {
        auto socket = std::make_unique<nx::network::BufferedStreamSocket>(httpClient->takeSocket());
        auto buffer = httpClient->fetchMessageBodyBuffer();

        if (buffer.size())
            socket->injectRecvData(std::move(buffer));

        return socket;
    }

    void parse(char* buffer, std::size_t bufferLength)
    {
        QnMutexLocker lock(&m_mutex);

        const std::size_t kTerminatorLength = 2;
        m_buffer.append(buffer, bufferLength);

        for (;;)
        {
            auto messageTerminatorPosition = m_buffer.indexOf(lit("\r\n"));

            if (messageTerminatorPosition == -1)
                return;

            const auto fullMessageLength = messageTerminatorPosition + kTerminatorLength;
            auto message = m_buffer.left(fullMessageLength);
            m_buffer.remove(0, fullMessageLength);

            m_messages.push(message);
        }
    }

    nx::Buffer getMessage()
    {
        nx::Buffer message;

        {
            QnMutexLocker lock(&m_mutex);
            if (!m_messages.empty())
            {
                message = m_messages.front();
                m_messages.pop();
            }
        }

        return message;
    }

    void launchTest(const QString& scheme)
    {
        const auto kRestHandlerPath = lit("/TakingHttpSocketTest");
        const int kAwaitedMessageNumber = 3;
        const int kPlayMessageNumber = 1;
        const int kTeardownMessageNumber = 2;

        auto httpServer = testHttpServer();
        ASSERT_TRUE(httpServer->registerRequestProcessor<TakingSocketRestHandler>(
            kRestHandlerPath,
            [this]()
            {
                return std::make_unique<TakingSocketRestHandler>(&m_threadStorage);
            })) << "Failed to register request processor";

        ASSERT_TRUE(httpServer->bindAndListen())
            << "Failed to start test http server";

        auto httpClient = std::make_unique<nx::network::http::HttpClient>();

        const nx::utils::Url url(lit("%1://%2%3")
            .arg(scheme)
            .arg(httpServer->serverAddress().toString())
            .arg(kRestHandlerPath));

        ASSERT_TRUE(httpClient->doGet(url)) << "Failed to perform GET request";

        std::unique_ptr<AbstractStreamSocket> tcpSocket;
        {
            std::unique_ptr<nx::network::http::HttpClient> localHttpClient;
            tcpSocket = takeSocketFromHttpClient(httpClient);
            std::swap(httpClient, localHttpClient);
        }

        while (m_threadStorage.empty())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        m_threadStorage.back().resume();

        ASSERT_TRUE(tcpSocket->isConnected())
            << "Socket taken from HTTP client is not connected";

        int bytesRead = 0;
        int messageNumber = 1;

        bool gotPlayMessage = false;
        bool gotTeardownMessage = false;

        while (tcpSocket->isConnected())
        {
            const std::size_t kReadBufferSize = 65536;
            char readBuffer[kReadBufferSize];

            bytesRead = tcpSocket->recv(readBuffer, kReadBufferSize);
            if (bytesRead > 0)
                parse(readBuffer, bytesRead);

            nx::Buffer message;
            while (!(message = getMessage()).isEmpty())
            {
                if (messageNumber == kPlayMessageNumber)
                {
                    ASSERT_EQ(kPlayMessage, message)
                        << "Expected '" << kPlayMessage.data() << "', got " << message.data();

                    gotPlayMessage = true;
                }
                else if (messageNumber == kTeardownMessageNumber)
                {
                    ASSERT_EQ(kTeardownMessage, message)
                        << "Expected '" << kTeardownMessage.data() << "', got " << message.data();

                    gotTeardownMessage = true;
                }
                else
                {
                    FAIL() << "Unexpected message: " << message.data();
                }

                ++messageNumber;
            }
        }

        ASSERT_EQ(messageNumber, kAwaitedMessageNumber)
            << "Got wrong number of messages. Expected: "
            << kAwaitedMessageNumber - 1
            << ", got: " << messageNumber - 1;

        ASSERT_TRUE(gotPlayMessage)
            << "Have not got '" << kPlayMessage.data() << "' message";

        ASSERT_TRUE(gotTeardownMessage)
            << "Have not got '" << kTeardownMessage.data() << "' message";;

        httpServer.reset();
        tcpSocket.reset();

        m_threadStorage.clear();
    }

private:
    nx::Buffer m_buffer;
    std::queue<nx::Buffer> m_messages;
    mutable QnMutex m_mutex;
    ThreadStorage m_threadStorage;
};

TEST_F(TakingHttpSocketTest, SslSocket)
{
    launchTest(nx::network::http::kSecureUrlSchemeName);
}

TEST_F(TakingHttpSocketTest, TcpSocket)
{
    launchTest(nx::network::http::kUrlSchemeName);
}

} // namespace nx
} // namespace network
} // namespace http
