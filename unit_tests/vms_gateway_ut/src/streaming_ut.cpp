#include "test_setup.h"

#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>

#include <vms_gateway_process.h>

#include "random_data_source.h"

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

namespace {

static const char* const kPath = "/Streaming";

} // namespace

class Streaming:
    public BasicComponentTest
{
protected:
    void startInfiniteStream()
    {
        ASSERT_TRUE(m_httpClient.doGet(contentUrl()));
    }

    void thenSomeStreamBytesAreReceivedByClient()
    {
        ASSERT_FALSE(m_httpClient.fetchMessageBodyBuffer().isEmpty());
    }

private:
    TestHttpServer m_httpServer;
    nx_http::HttpClient m_httpClient;

    virtual void SetUp() override
    {
        m_httpClient.setSendTimeoutMs(0);
        m_httpClient.setResponseReadTimeoutMs(0);
        m_httpClient.setMessageBodyReadTimeoutMs(0);

        addArg("--tcp/recvTimeout=1h");
        addArg("--tcp/sendTimeout=1h");

        ASSERT_TRUE(startAndWaitUntilStarted());
        launchHttpServer();
    }

    void launchHttpServer()
    {
        m_httpServer.registerContentProvider(
            kPath,
            std::bind(&Streaming::createContentProvider, this));

        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    std::unique_ptr<nx_http::AbstractMsgBodySource> createContentProvider()
    {
        return std::make_unique<RandomDataSource>("video/mp2t");
    }

    QUrl contentUrl() const
    {
        return QUrl(lm("http://%1/%2%3").arg(moduleInstance()->impl()->httpEndpoints()[0])
            .arg(m_httpServer.serverAddress()).arg(kPath));
    }
};

TEST_F(Streaming, DISABLED_stream_bytes_arrive_to_client_before_stream_end)
{
    startInfiniteStream();
    thenSomeStreamBytesAreReceivedByClient();
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
