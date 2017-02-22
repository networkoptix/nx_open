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

class TakingSocketRestHandler: public nx_http::AbstractHttpRequestHandler
{
    
public:

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler) override
    {
        nx_http::ConnectionEvents events;
        events.onResponseHasBeenSent = 
            [](nx_http::HttpServerConnection* connection)
            {
                auto socket = connection->takeSocket();
                nx::utils::thread serverThread(
                    [sock = std::move(socket)]()
                    {
                        int result = 0;
                        sock->setNonBlockingMode(false);

                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        result = sock->send(kPlayMessage);
                        
                        ASSERT_GT(result, 0);
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        result = sock->send(kTeardownMessage);

                        ASSERT_GT(result, 0);
                        std::this_thread::sleep_for(std::chrono::seconds(1));   
                    });
                
                serverThread.detach();
            };

        completionHandler(
            nx_http::RequestResult(
                nx_http::StatusCode::ok,
                std::make_unique<nx_http::EmptyMessageBodySource>(nx::String(), boost::none),
                std::move(events)));
    }
};

class TakingHttpSocketTest: public ::testing::Test
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

        while (true)
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
        const auto kRestHandlerPath = lit("/test");
        const std::size_t kReadBufferSize = 65536;
        char readBuffer[kReadBufferSize];

        const int kTimeoutMs(1000);
        const int kMessageReadTimeoutMs(5000);
        const int kAwaitedMessageNumber = 3;

        std::unique_ptr<nx_http::HttpClient> httpClient;
        std::unique_ptr<AbstractStreamSocket> tcpSocket;

        const int kPlayMessageNumber = 1;
        const int kTeardownMessageNumber = 2;

        bool gotPlayMessage = false;
        bool gotTeardownMessage = false;

        std::unique_ptr<TestHttpServer> httpServer;

        for (int i = 0; i < 3; ++i)
        {
            httpServer = testHttpServer();
            ASSERT_TRUE(httpServer->registerRequestProcessor<TakingSocketRestHandler>(kRestHandlerPath))
                << "Failed to register request processor";

            ASSERT_TRUE(httpServer->bindAndListen())
                << "Failed to start test http server";

            {
                std::unique_ptr<nx_http::HttpClient> localHttpClient =
                    std::make_unique<nx_http::HttpClient>();

                tcpSocket.reset();
                std::swap(httpClient, localHttpClient);
            }

            httpClient->setSendTimeoutMs(kTimeoutMs);
            httpClient->setResponseReadTimeoutMs(kTimeoutMs);
            httpClient->setMessageBodyReadTimeoutMs(kTimeoutMs);

            const QUrl url(lit("%1://%2%3")
                .arg(scheme)
                .arg(httpServer->serverAddress().toString())
                .arg(kRestHandlerPath));

            ASSERT_TRUE(httpClient->doGet(url))
                << "Failed to perform GET request";

            {
                std::unique_ptr<nx_http::HttpClient> localHttpClient;
                tcpSocket = takeSocketFromHttpClient(httpClient);
                std::swap(httpClient, localHttpClient);
            }

            ASSERT_TRUE(tcpSocket->isConnected())
                << "Socket taken from HTTP client is not connected";

            QElapsedTimer timer;
            timer.start();

            int bytesRead = 0;
            int messageNumber = 1;

            gotPlayMessage = false;
            gotTeardownMessage = false;

            while (tcpSocket->isConnected() && timer.elapsed() < kMessageReadTimeoutMs)
            {
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

            decltype(httpClient) localHttpClient;
            std::swap(httpClient, localHttpClient);
            httpServer.reset();
            tcpSocket.reset();
        }
    }

private:
    nx::Buffer m_buffer;
    std::queue<nx::Buffer> m_messages;
    mutable QnMutex m_mutex;
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
