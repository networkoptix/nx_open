// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "test_setup.h"

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>

#include <nx/cloud/vms_gateway/vms_gateway_process.h>

#include "random_data_source.h"

namespace nx {
namespace cloud {
namespace gateway {
namespace test {

namespace {

static const char* const kStreamPath = "/streaming";
static const char* const kFixedResourcePath = "/fixed";

} // namespace

class Streaming:
    public BasicComponentTest
{
public:
    Streaming()
    {
        m_fixedResource.resize(16*1024);
        std::generate(m_fixedResource.begin(), m_fixedResource.end(), &rand);
    }

protected:
    void startInfiniteStream()
    {
        ASSERT_TRUE(m_httpClient.doGet(streamUrl()));
    }

    void whenRequestedFixedResource()
    {
        ASSERT_TRUE(m_httpClient.doGet(fixedResourceUrl()));
    }

    void thenSomeStreamBytesAreReceivedByClient()
    {
        ASSERT_FALSE(m_httpClient.fetchMessageBodyBuffer().empty());
    }

    void thenResourceHasBeenReceived()
    {
        nx::Buffer msgBody;
        while (!m_httpClient.eof())
            msgBody += m_httpClient.fetchMessageBodyBuffer();
        ASSERT_EQ(m_fixedResource, msgBody);
    }

private:
    nx::network::http::TestHttpServer m_httpServer;
    nx::network::http::HttpClient m_httpClient{nx::network::ssl::kAcceptAnyCertificate};
    nx::Buffer m_fixedResource;

    virtual void SetUp() override
    {
        m_httpClient.setSendTimeout(nx::network::kNoTimeout);
        m_httpClient.setResponseReadTimeout(nx::network::kNoTimeout);
        m_httpClient.setMessageBodyReadTimeout(nx::network::kNoTimeout);

        addArg("--tcp/recvTimeout=1h");
        addArg("--tcp/sendTimeout=1h");

        ASSERT_TRUE(startAndWaitUntilStarted());
        launchHttpServer();
    }

    void launchHttpServer()
    {
        m_httpServer.registerContentProvider(
            kStreamPath,
            std::bind(&Streaming::createStreamContentProvider, this));

        m_httpServer.registerContentProvider(
            kFixedResourcePath,
            std::bind(&Streaming::createFixedContentProvider, this));

        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    std::unique_ptr<nx::network::http::AbstractMsgBodySource> createStreamContentProvider()
    {
        return std::make_unique<RandomDataSource>("video/mp2t");
    }

    std::unique_ptr<nx::network::http::AbstractMsgBodySource> createFixedContentProvider()
    {
        return std::make_unique<nx::network::http::BufferSource>("video/mp2t", m_fixedResource);
    }

    nx::utils::Url streamUrl() const
    {
        return nx::utils::Url(nx::format("http://%1/%2%3").arg(moduleInstance()->impl()->httpEndpoints()[0])
            .arg(m_httpServer.serverAddress()).arg(kStreamPath));
    }

    nx::utils::Url fixedResourceUrl() const
    {
        return nx::utils::Url(nx::format("http://%1/%2%3").arg(moduleInstance()->impl()->httpEndpoints()[0])
            .arg(m_httpServer.serverAddress()).arg(kFixedResourcePath));
    }
};

TEST_F(Streaming, stream_bytes_arrive_to_client_before_stream_end)
{
    startInfiniteStream();
    thenSomeStreamBytesAreReceivedByClient();
}

TEST_F(Streaming, streaming_multiple_messages_through_a_single_connection)
{
    for (int i = 0; i < 7; ++i)
    {
        whenRequestedFixedResource();
        thenResourceHasBeenReceived();
    }
}

} // namespace test
} // namespace gateway
} // namespace cloud
} // namespace nx
