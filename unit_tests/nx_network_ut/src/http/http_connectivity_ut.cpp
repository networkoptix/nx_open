#include <chrono>
#include <memory>
#include <queue>

#include <QtCore/QElapsedTimer>
#include <gtest/gtest.h>

#include <nx/network/http/httpclient.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/std/thread.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/empty_message_body_source.h>
#include <nx/network/buffered_stream_socket.h>

namespace nx_http {

static const nx::Buffer kPlayMessage("PLAY\r\n");
static const nx::Buffer kTeardownMessage("TEARDOWN\r\n");

class ThreadStorage
{
public:
    ~ThreadStorage()
    {
        for (auto& thread: m_threads)
            thread.join();
    }

    void add(nx::utils::thread thread)
    {
        QnMutexLocker lock(&m_mutex);
        m_threads.emplace_back(std::move(thread));
    }

private:
    QnMutex m_mutex;
    std::vector<nx::utils::thread> m_threads;
};

class TakingSocketRestHandler:
    public nx_http::AbstractHttpRequestHandler
{
public:
    TakingSocketRestHandler(ThreadStorage* threadStorage):
        m_threadStorage(threadStorage)
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler) override
    {
        nx_http::ConnectionEvents events;
        events.onResponseHasBeenSent = 
            [threadStorage = m_threadStorage](nx_http::HttpServerConnection* connection)
            {
                threadStorage->add(nx::utils::thread(
                    [sock = connection->takeSocket()]() mutable
                    {
                        ASSERT_TRUE(sock->setNonBlockingMode(false));
                        ASSERT_GT(sock->send(kPlayMessage), 0);
                        ASSERT_GT(sock->send(kTeardownMessage), 0);
                        sock.reset();
                    }));
            };

        completionHandler(
            nx_http::RequestResult(
                nx_http::StatusCode::ok,
                std::make_unique<nx_http::EmptyMessageBodySource>(nx::String(), boost::none),
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
        std::unique_ptr<nx_http::HttpClient>& httpClient)
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

        for (int i = 0; i < 20; ++i)
        {
            auto httpServer = testHttpServer();
            ASSERT_TRUE(httpServer->registerRequestProcessor<TakingSocketRestHandler>(
                kRestHandlerPath,
                [this]()
                {
                    return std::make_unique<TakingSocketRestHandler>(&m_threadStorage);
                })) << "Failed to register request processor";

            ASSERT_TRUE(httpServer->bindAndListen())
                << "Failed to start test http server";

            auto httpClient = std::make_unique<nx_http::HttpClient>();

            const QUrl url(lit("%1://%2%3")
                .arg(scheme)
                .arg(httpServer->serverAddress().toString())
                .arg(kRestHandlerPath));

            ASSERT_TRUE(httpClient->doGet(url)) << "Failed to perform GET request "<<i;

            std::unique_ptr<AbstractStreamSocket> tcpSocket;
            {
                std::unique_ptr<nx_http::HttpClient> localHttpClient;
                tcpSocket = takeSocketFromHttpClient(httpClient);
                std::swap(httpClient, localHttpClient);
            }

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
        }
    }

private:
    nx::Buffer m_buffer;
    std::queue<nx::Buffer> m_messages;
    mutable QnMutex m_mutex;
    ThreadStorage m_threadStorage;
};

TEST_F(TakingHttpSocketTest, SslSocket)
{
    launchTest(lit("https"));
}

TEST_F(TakingHttpSocketTest, TcpSocket)
{
    launchTest(lit("http"));
} 

} // namespace nx_http
