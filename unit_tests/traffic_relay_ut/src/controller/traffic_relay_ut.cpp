#include <atomic>

#include <thread>

#include <gtest/gtest.h>

#include <nx/network/aio/test/aio_test_async_channel.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/random.h>
#include <nx/utils/string.h>
#include <nx/utils/test_support/test_pipeline.h>

#include <nx/cloud/relay/controller/traffic_relay.h>

namespace nx {
namespace cloud {
namespace relay {
namespace controller {
namespace test {

namespace {

static std::atomic<std::size_t> asyncChannelInstanceCount(0);

class AsyncChannel:
    public network::aio::test::AsyncChannel
{
    using base_type = network::aio::test::AsyncChannel;

public:
    AsyncChannel(
        utils::bstream::AbstractInput* input,
        utils::bstream::AbstractOutput* output)
        :
        base_type(input, output, InputDepletionPolicy::retry)
    {
        ++asyncChannelInstanceCount;
    }

    ~AsyncChannel()
    {
        --asyncChannelInstanceCount;
    }
};

} // namespace

class TrafficRelay:
    public ::testing::Test
{
public:
    TrafficRelay()
    {
        m_serverPeerId = nx::utils::generateRandomName(7).toStdString();
    }

protected:
    void givenStreamSocketsBeingRelayed()
    {
        m_trafficRelay.startRelaying(
            {std::move(m_clientChannel.channel), ""},
            {std::move(m_serverChannel.channel), m_serverPeerId});
    }

    void whenEitherSocketIsClosed()
    {
        AsyncChannel* channelToClose = nullptr;
        if (nx::utils::random::number<int>(0, 1) > 0)
            channelToClose = m_clientChannel.channelPtr;
        else
            channelToClose = m_serverChannel.channelPtr;
        channelToClose->setReadErrorState(SystemError::connectionReset);
    }

    void thenTrafficBetweenStreamSocketsIsRelayed()
    {
        const QByteArray testMessage = nx::utils::random::generate(
            nx::utils::random::number(4096, 128*1024));

        m_clientChannel.input.write(testMessage.data(), testMessage.size());
        m_serverChannel.output.waitForReceivedDataToMatch(testMessage);
    }

    void thenTrafficRelayClosesAnotherOne()
    {
        while (asyncChannelInstanceCount > 0)
            std::this_thread::yield();
    }

private:
    struct ChannelContext
    {
        utils::bstream::Pipe input;
        utils::bstream::test::NotifyingOutput output;
        std::unique_ptr<AsyncChannel> channel;
        AsyncChannel* channelPtr;

        ChannelContext():
            channel(std::make_unique<AsyncChannel>(&input, &output)),
            channelPtr(channel.get())
        {
        }
    };

    ChannelContext m_clientChannel;
    ChannelContext m_serverChannel;
    std::string m_serverPeerId;
    controller::TrafficRelay m_trafficRelay;
};

TEST_F(TrafficRelay, actually_relays_data)
{
    givenStreamSocketsBeingRelayed();
    thenTrafficBetweenStreamSocketsIsRelayed();
}

TEST_F(TrafficRelay, relaying_is_stopped_with_either_connection_closure)
{
    givenStreamSocketsBeingRelayed();
    whenEitherSocketIsClosed();
    thenTrafficRelayClosesAnotherOne();
}

} // namespace test
} // namespace controller
} // namespace relay
} // namespace cloud
} // namespace nx
