#include <gtest/gtest.h>
#include <nx/network/cloud/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/test_support/stun_async_client_mock.h>
#include <utils/thread/sync_queue.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

class AddressResolverTest
:
    public ::testing::Test,
    protected AddressResolver
{
public:
    static void SetUpTestCase()
    {
        s_stunClient = std::make_shared<stun::test::AsyncClientMock>();
        s_stunClient->emulateRequestHandler(
            stun::cc::methods::resolvePeer,
            [](
                stun::Message request,
                stun::AbstractAsyncClient::RequestHandler handler )
            {
                const auto attr = request.getAttribute<stun::cc::attrs::HostName>();
                ASSERT_TRUE(attr);

                stun::Message response(stun::Header(
                    stun::MessageClass::successResponse,
                    stun::cc::methods::resolvePeer,
                    std::move(request.header.transactionId)));

                const auto it = s_endpoints.find(attr->getString());
                if (it == s_endpoints.end())
                {
                    response.header.messageClass = stun::MessageClass::errorResponse;
                    response.newAttribute<stun::attrs::ErrorDescription>(404);
                }
                else
                {
                    response.newAttribute<stun::cc::attrs::PublicEndpointList>(it->second);
                    response.newAttribute<stun::cc::attrs::ConnectionMethods>("1");
                }

                handler(SystemError::noError, std::move(response));
            });
    }

    static void emulateAddress(
        const HostAddress& address,
        std::list<SocketAddress> endpoints = {})
    {
        if (endpoints.size())
            s_endpoints.emplace(address.toString(), std::move(endpoints));
        else
            s_endpoints.erase(address.toString());
    }

    static void TearDownTestCase()
    {
        s_stunClient.reset();
        s_endpoints.clear();
    }

    AddressResolverTest()
    :
        AddressResolver(
            std::make_shared<hpm::api::MediatorClientTcpConnection>(s_stunClient))
    {
    }

    ~AddressResolverTest() override
    {
        pleaseStopSync();
    }

    void resolveAndCheckState(
        const HostAddress& address,
        utils::MoveOnlyFunc<void(HaInfoIterator it)> checker)
    {
        nx::TestSyncQueue<bool> syncQueue;
        static const size_t kSimultaneousQueries = 100;
        for (size_t counter = kSimultaneousQueries; counter; --counter)
        {
            resolveAsync(
                address,
                [&](SystemError::ErrorCode, std::vector<AddressEntry>)
                {
                    {
                        QnMutexLocker lk(&m_mutex);
                        checker(m_info.find(address));
                    }

                    syncQueue.push(true);
                });
        }

        for (size_t counter = kSimultaneousQueries; counter; --counter)
            syncQueue.pop();
    }

private:
    static std::map<QString, std::list<SocketAddress>> s_endpoints;
    static std::shared_ptr<stun::test::AsyncClientMock> s_stunClient;
};

std::map<QString, std::list<SocketAddress>> AddressResolverTest::s_endpoints;
std::shared_ptr<stun::test::AsyncClientMock> AddressResolverTest::s_stunClient;

static const HostAddress kAddress("ya.ru");
static const SocketAddress kResult("10.11.12.13:12345");

/**
 * Usual DNS addresses like "ya.ru" shell be resolved in order:
 *  fixed -> dns -> mediator (never touches mediator because of valid DNS)
 */
TEST_F(AddressResolverTest, FixedVsDns)
{
    addFixedAddress(kAddress, kResult);
    resolveAndCheckState(
        kAddress, [&](HaInfoIterator it)
        {
            const HostAddressInfo& info = it->second;
            ASSERT_EQ(info.fixedEntries.size(), 1);

            const AddressEntry& entry = info.fixedEntries.front();
            EXPECT_EQ(entry.type, AddressType::direct);
            EXPECT_EQ(entry.host, kResult.address);
            EXPECT_EQ(entry.attributes.size(), 1);
            EXPECT_EQ(entry.attributes.front().type, AddressAttributeType::port);
            EXPECT_EQ(entry.attributes.front().value, kResult.port);

            EXPECT_EQ(info.dnsState(), HostAddressInfo::State::unresolved);
            EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::unresolved);
        });

    size_t yandexIpCount = 0;
    removeFixedAddress(kAddress, kResult);
    resolveAndCheckState(
        kAddress, [&](HaInfoIterator it)
        {
            const HostAddressInfo& info = it->second;
            EXPECT_EQ(info.fixedEntries.size(), 0);
            EXPECT_EQ(info.dnsState(), HostAddressInfo::State::resolved);
            EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::unresolved);

            const auto entries = info.getAll();
            yandexIpCount = entries.size();
            EXPECT_GT(yandexIpCount, 0);
            for (const auto& e : entries)
                EXPECT_EQ(e.type, AddressType::direct);
        });

    addFixedAddress(kAddress, kResult);
    resolveAndCheckState(
        kAddress, [&](HaInfoIterator it)
        {
            const HostAddressInfo& info = it->second;
            ASSERT_EQ(info.fixedEntries.size(), 1);

            const AddressEntry& entry = info.fixedEntries.front();
            EXPECT_EQ(entry.type, AddressType::direct);
            EXPECT_EQ(entry.host, kResult.address);
            ASSERT_EQ(entry.attributes.size(), 1);
            EXPECT_EQ(entry.attributes.front().type, AddressAttributeType::port);
            EXPECT_EQ(entry.attributes.front().value, kResult.port);

            EXPECT_EQ(info.dnsState(), HostAddressInfo::State::resolved);
            EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::unresolved);
            EXPECT_EQ(info.getAll().size(), 1 + yandexIpCount); // still have them
        });
}

static QString testUuid()
{
    return QnUuid::createUuid().toSimpleString();
}

/**
 * Cloud-like adresses shell be resolved in next order:
 *  fixed -> mediator -> dns
 */
TEST_F(AddressResolverTest, FixedVsMediatorVsDns)
{
    const std::vector<HostAddress> kGoodCloudAddresses =
    {
        HostAddress(lm("server.%1").arg(testUuid())),
        HostAddress(lm("%1.%2").arg(testUuid()).arg(testUuid())),
    };

    for (const auto& host : kGoodCloudAddresses)
    {
        emulateAddress(host, {kResult});
        resolveAndCheckState(
            host, [&](HaInfoIterator it)
            {
                const HostAddressInfo& info = it->second;
                EXPECT_EQ(info.fixedEntries.size(), 0);
                EXPECT_EQ(info.dnsState(), HostAddressInfo::State::unresolved);
                EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::resolved);

                const auto entries = info.getAll();
                ASSERT_EQ(info.getAll().size(), 2);

                const AddressEntry entry1 = entries.front();
                EXPECT_EQ(entry1.type, AddressType::direct);
                EXPECT_EQ(entry1.host, kResult.address);
                EXPECT_EQ(entry1.attributes.size(), 1);
                EXPECT_EQ(entry1.attributes.front().type, AddressAttributeType::port);
                EXPECT_EQ(entry1.attributes.front().value, kResult.port);

                const AddressEntry entry2 = entries.back();
                EXPECT_EQ(entry2.type, AddressType::cloud);
                EXPECT_EQ(entry2.host, host);
            });

        addFixedAddress(host, kResult);
        resolveAndCheckState(
            host, [&](HaInfoIterator it)
            {
                const HostAddressInfo& info = it->second;
                ASSERT_EQ(info.fixedEntries.size(), 1);

                const AddressEntry& entry = info.fixedEntries.front();
                EXPECT_EQ(entry.type, AddressType::direct);
                EXPECT_EQ(entry.host, kResult.address);
                ASSERT_EQ(entry.attributes.size(), 1);
                EXPECT_EQ(entry.attributes.front().type, AddressAttributeType::port);
                EXPECT_EQ(entry.attributes.front().value, kResult.port);

                EXPECT_EQ(info.getAll().size(), 3); // still have some from mediator
                EXPECT_EQ(info.dnsState(), HostAddressInfo::State::unresolved);
                EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::resolved);
            });
    }

    const std::vector<HostAddress> kBadCloudAddresses =
    {
        HostAddress(lm("server.%1").arg(testUuid())),
        HostAddress(lm("%1.%2").arg(testUuid()).arg(testUuid())),
    };

    for (const auto& host : kBadCloudAddresses)
    {
        resolveAndCheckState(
            host, [&](HaInfoIterator it)
            {
                const HostAddressInfo& info = it->second;
                EXPECT_EQ(info.fixedEntries.size(), 0);
                EXPECT_EQ(info.getAll().size(), 0);
                EXPECT_EQ(info.dnsState(), HostAddressInfo::State::resolved);
                EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::resolved);
            });
    }
}

/**
 * Usual DNS addresses like "ya.ru" shell be resolved in order:
 *  fixed -> dns -> mediator (always uses mediator because of not valid DNS)
 */
TEST_F(AddressResolverTest, DnsVsMediator)
{
    static const HostAddress kAddressGood("hren-resolve-me-1.com");
    static const HostAddress kAddressBad("hren-resolve-me-2.com");

    emulateAddress(kAddressGood, {kResult});
    resolveAndCheckState(
        kAddressGood, [&](HaInfoIterator it)
        {
            const HostAddressInfo& info = it->second;
            EXPECT_EQ(info.fixedEntries.size(), 0);
            EXPECT_EQ(info.dnsState(), HostAddressInfo::State::resolved);
            EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::resolved);

            const auto entries = info.getAll();
            ASSERT_EQ(info.getAll().size(), 2);

            const AddressEntry entry1 = entries.front();
            EXPECT_EQ(entry1.type, AddressType::direct);
            EXPECT_EQ(entry1.host, kResult.address);
            EXPECT_EQ(entry1.attributes.size(), 1);
            EXPECT_EQ(entry1.attributes.front().type, AddressAttributeType::port);
            EXPECT_EQ(entry1.attributes.front().value, kResult.port);

            const AddressEntry entry2 = entries.back();
            EXPECT_EQ(entry2.type, AddressType::cloud);
            EXPECT_EQ(entry2.host, kAddressGood);
        });

    resolveAndCheckState(
        kAddressBad, [&](HaInfoIterator it)
        {
            const HostAddressInfo& info = it->second;
            EXPECT_EQ(info.fixedEntries.size(), 0);
            EXPECT_EQ(info.getAll().size(), 0);
            EXPECT_EQ(info.dnsState(), HostAddressInfo::State::resolved);
            EXPECT_EQ(info.mediatorState(), HostAddressInfo::State::resolved);
        });
}

TEST(AddressResolverRealTest, Cancel)
{
    const auto doNone = [&](SystemError::ErrorCode, std::vector<AddressEntry>) {};

    const std::vector<HostAddress> kTestAddresses =
    {
        HostAddress("ya.ru"),
        HostAddress("hren-resolve-me-1.com"),
        HostAddress(lm("%1.%2").arg(testUuid()).arg(testUuid())),
    };

    for (size_t timeout = 0; timeout <= 5000; timeout *= 2)
    {
        for (const auto& address : kTestAddresses)
        {
            SocketGlobals::addressResolver().resolveAsync(address, doNone, true, this);
            if (timeout) std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
            SocketGlobals::addressResolver().cancel(this);
            if (!timeout) timeout = 10;
        }
    }
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
