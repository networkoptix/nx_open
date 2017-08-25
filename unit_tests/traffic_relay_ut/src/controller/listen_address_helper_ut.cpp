#include <gtest/gtest.h>
#include <controller/listen_address_helper.h>
#include <nx/network/socket_common.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {
namespace test {

class TestAbstractListenAddressHelperHandler: public AbstractListenAddressHelperHandler
{
public:
    virtual void onBestEndpointDiscovered(std::string endpoint) override
    {
        m_endpoint = std::move(endpoint);
    }

    std::string endpoint() const { return m_endpoint; }

private:
    std::string m_endpoint;
};

class ListenAddressHelperStub: public AbstractListenAddressHelper
{
public:
    ListenAddressHelperStub(AbstractListenAddressHelperHandler* handler):
        AbstractListenAddressHelper(handler)
    {}

    void setPublicAddress(const HostAddress& address)
    {
        m_address = address;
    }

private:
    HostAddress m_address;

    virtual boost::optional<HostAddress> publicAddress() const override
    {
        return m_address;
    }
};

static const HostAddress kPublicAddress{QString("1.1.1.1")};

class ListenAddressHelper: public ::testing::Test
{
protected:
    ListenAddressHelper(): m_addressHelper(&m_addressHelperHandler)
    {}

    void whenPublicAddressFound()
    {
        m_addressHelper.setPublicAddress(kPublicAddress);
    }

    void whenSomeEndpointsFoundWithPublicAddressAmongThem()
    {
        m_endpoints.emplace_back(SocketAddress("10.3.0.7", 11));
        m_endpoints.emplace_back(SocketAddress("127.0.0.1", 11));
        m_endpoints.emplace_back(SocketAddress("1.1.1.1", 21));

        m_addressHelper.findBestAddress(m_endpoints);
    }

    void thenListenEndpointAddressReportedShouldBeEqualToThePublicAddress()
    {
        ASSERT_NE(
            m_addressHelperHandler.endpoint().find(kPublicAddress.toString().toStdString()),
            std::string::npos);
    }

    void thenFirstEndpointShouldBeReported()
    {
        ASSERT_EQ("10.3.0.7:11", m_addressHelperHandler.endpoint());
    }

private:
    TestAbstractListenAddressHelperHandler m_addressHelperHandler;
    ListenAddressHelperStub m_addressHelper;
    std::vector<SocketAddress> m_endpoints;
};


TEST_F(ListenAddressHelper, publicAddressFound_andIsInTheEndpoints)
{
    whenPublicAddressFound();
    whenSomeEndpointsFoundWithPublicAddressAmongThem();

    thenListenEndpointAddressReportedShouldBeEqualToThePublicAddress();
}

TEST_F(ListenAddressHelper, publicAddressNotFound_andIsInTheEndpoints)
{
    whenSomeEndpointsFoundWithPublicAddressAmongThem();
    thenFirstEndpointShouldBeReported();
}

} // namespace test
} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx