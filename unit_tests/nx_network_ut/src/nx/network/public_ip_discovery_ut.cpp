#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/public_ip_discovery.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/utils.h>

namespace nx {
namespace network {
namespace test {

static const QString kPublicIpAddress(lit("127.0.0.1"));

class PublicIPDiscovery:
    public ::testing::Test
{
public:
    PublicIPDiscovery():
        m_testHttpServer(std::make_unique<http::TestHttpServer>())
    {
        init();
    }

protected:
    enum EventHandlerOptionFlag
    {
        withDelay = 1,
    };

    http::TestHttpServer* testHttpServer()
    {
        return m_testHttpServer.get();
    }

    QString serverUrl() const
    {
        return lit("http://%1/test").arg(m_testHttpServer->serverAddress().toString());
    }

    void registerEventHandler(int eventHandlerOptions = 0)
    {
        QObject::connect(
            m_publicAddressFinder.get(), &::nx::network::PublicIPDiscovery::found,
            [this, eventHandlerOptions](const QHostAddress& address)
            {
                m_eventHandlerHasBeenInvoked.set_value();
                if (eventHandlerOptions & withDelay)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                m_foundIpAddress = address.toString();
                m_finderHasFinished.set_value();
            });
    }

    void invokeFinder()
    {
        m_publicAddressFinder->update();
        m_eventHandlerHasBeenInvoked.get_future().wait();
    }

    void destroyFinder()
    {
        m_publicAddressFinder.reset();
    }

    void assertIfFinderDidNotWaitForEventHandlerToReturn()
    {
        ASSERT_EQ(
            std::future_status::ready,
            m_finderHasFinished.get_future().wait_for(std::chrono::seconds::zero()));
    }

    void assertIfIpAddressHasNotBeenFound()
    {
        m_finderHasFinished.get_future().wait();
        ASSERT_TRUE(static_cast<bool>(m_foundIpAddress));
        ASSERT_EQ(kPublicIpAddress, *m_foundIpAddress);
    }

private:
    std::unique_ptr<http::TestHttpServer> m_testHttpServer;
    std::unique_ptr<nx::network::PublicIPDiscovery> m_publicAddressFinder;
    nx::utils::promise<void> m_finderHasFinished;
    nx::utils::promise<void> m_eventHandlerHasBeenInvoked;
    boost::optional<QString> m_foundIpAddress;

    void init()
    {
        m_testHttpServer->registerStaticProcessor("/test", kPublicIpAddress.toLatin1(), "text/plain");
        ASSERT_TRUE(m_testHttpServer->bindAndListen())
            << SystemError::getLastOSErrorText().toStdString();

        m_publicAddressFinder = std::make_unique<::nx::network::PublicIPDiscovery>(
            QStringList() << serverUrl());
    }
};

TEST_F(PublicIPDiscovery, finds_ip_address)
{
    registerEventHandler();
    invokeFinder();
    assertIfIpAddressHasNotBeenFound();
}

TEST_F(PublicIPDiscovery, cancellation)
{
    registerEventHandler(withDelay);
    invokeFinder();
    destroyFinder();
    assertIfFinderDidNotWaitForEventHandlerToReturn();
}

} // namespace test
} // namespace network
} // namespace nx
