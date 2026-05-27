// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include <nx/network/test_support/dummy_socket.h>
#include <nx/ranges.h>
#include <nx/rtp/sdp.h>
#include <nx/streaming/rtsp_client.h>
#include <nx/utils/scope_guard.h>

TEST(Sdp, SetupResponse1)
{
    using namespace nx::vms::api;
    using namespace std::chrono;

    QString value =
        "RTSP/1.0 200 OK\r\n"
        "CSeq: 25\r\n"
        "Session: SomeId13816;timeout=50\r\n"
        "Transport: RTP/AVP;multicast;destination=239.128.1.100;port=5564-5565;ttl=15;interleaved=2-3;ssrc=1234;\r\n"
        "\r\n";

    QnRtspClient rtspClient(QnRtspClient::Config{});
    QnRtspClient::SDPTrackInfo track;
    track.ioDevice.reset(new QnRtspIoDevice(&rtspClient, RtpTransportType::multicast));
    rtspClient.parseSetupResponse(value, &track, 0);

    ASSERT_EQ("239.128.1.100", track.sdpMedia.connectionAddress.toString());
    ASSERT_EQ(5564, track.ioDevice->mediaAddressInfo().address.port);
    ASSERT_EQ(5565, track.ioDevice->rtcpAddressInfo().address.port);

    ASSERT_EQ(2, track.interleaved.first);
    ASSERT_EQ(3, track.interleaved.second);

    ASSERT_EQ(0x1234, track.ioDevice->getSSRC());

    ASSERT_EQ("SomeId13816", rtspClient.sessionId());
    ASSERT_EQ(50s, rtspClient.keepAliveTimeOut());
}

TEST(Sdp, SetupResponse2)
{
    using namespace nx::vms::api;
    using namespace std::chrono;

    QString value =
        "RTSP/1.0 200 OK\r\n"
        "CSeq: 25\r\n"
        "Session: 13816;timeout=50\r\n"
        "Transport: RTP/AVP; multicast; destination=239.128.1.100; Server_Port=5564; ttl=15; Interleaved=2\r\n"
        "\r\n";

    QnRtspClient rtspClient(QnRtspClient::Config{});
    QnRtspClient::SDPTrackInfo track;
    track.ioDevice.reset(new QnRtspIoDevice(&rtspClient, RtpTransportType::multicast));
    rtspClient.parseSetupResponse(value, &track, 0);

    ASSERT_EQ(2, track.interleaved.first);
    ASSERT_EQ(3, track.interleaved.second);

    ASSERT_EQ(5564, track.ioDevice->mediaAddressInfo().address.port);
    ASSERT_EQ(5565, track.ioDevice->rtcpAddressInfo().address.port);
}

TEST(Rtsp, ParseTextMessage)
{
    {
        std::string_view data("RTSP/1.0 200 OK\r\nSession: 13816;timeout=50\r\n\r\n");
        ASSERT_EQ(data.size(), nextRtspMessage((uint8_t*)data.data(), data.size(), /*maxChannelNumber*/ 256));
    }
    {
        // No channel number in buffer -> no message.
        std::string_view data("RTSP/1.0 200 OK$");
        ASSERT_EQ(0, nextRtspMessage((uint8_t*)data.data(), data.size(), /*maxChannelNumber*/ 256));
    }
    {
        // Channel number too big -> no message.
        std::string_view data("RTSP/1.0 200 OK$q");
        ASSERT_EQ(0, nextRtspMessage((uint8_t*)data.data(), data.size(), /*maxChannelNumber*/ 0));
    }
    {
        // Normal message.
        std::string_view data("RTSP/1.0 200 OK$0");
        ASSERT_EQ(data.size() - 2, nextRtspMessage((uint8_t*)data.data(), data.size(), /*maxChannelNumber*/ 256));
    }
    {
        // Normal message before $.
        std::string_view data("RTSP/1.0 200 OK\r\n\r\nq$0");
        ASSERT_EQ(data.size() - 3, nextRtspMessage((uint8_t*)data.data(), data.size(), /*maxChannelNumber*/ 256));
    }
    {
        // Normal message.
        std::string_view data("K$0");
        ASSERT_EQ(1, nextRtspMessage((uint8_t*)data.data(), data.size(), /*maxChannelNumber*/ 256));
    }
    {
        // No double delimiter -> no message.
        std::string_view data("RTSP/1.0 200 OK\r\n");
        ASSERT_EQ(0, nextRtspMessage((uint8_t*)data.data(), data.size(), /*maxChannelNumber*/ 256));
    }
    {
        std::string_view data("");
        ASSERT_EQ(0, nextRtspMessage((uint8_t*)data.data(), data.size(), /*maxChannelNumber*/ 256));
    }
    {
        std::string_view data("$");
        ASSERT_EQ(0, nextRtspMessage((uint8_t*)data.data(), data.size(), /*maxChannelNumber*/ 256));
    }
    {
        std::string_view data("\r");
        ASSERT_EQ(0, nextRtspMessage((uint8_t*)data.data(), data.size(), /*maxChannelNumber*/ 256));
    }
}

namespace {

// TODO: #skolesnik This class is copy-paste. Refactor and reuse from a common header.
// TODO: #skolesnik Use std::stacktrace
class TracedName
{
public:
    // Use TRACED_NAME macro.
    TracedName(std::string name, std::string filename, int line):
        m_name(std::move(name)), m_filename(std::move(filename)), m_lineNumber(line)
    {
    }

    TracedName(const TracedName&) = default;
    TracedName(TracedName&&) = delete;

    std::string_view operator()() const { return m_name; }

    [[nodiscard]] ::testing::ScopedTrace trace() const
    {
        return {m_filename.c_str(), m_lineNumber, m_name.c_str()};
    }

private:
    std::string m_name;
    std::string m_filename;
    int m_lineNumber;
};

// Allows to jump to Test Case declaration line.
#define TRACED_NAME(name) TracedName(name, __FILE__, __LINE__)

struct RtspStreamMessage
{
    std::string bytes;
};

// TODO: #skolesnik Support other valid RTSP interleaved frame payload shapes.
RtspStreamMessage validFrame(std::string_view payload, std::uint8_t channelId = 0)
{
    const FrameHeader header{
        .marker = '$',
        .channelId = channelId,
        // RTSP interleaved frame length is serialized in big-endian byte order.
        .payloadSize = std::invoke(
            [size = static_cast<std::uint16_t>(payload.size())]<auto endian = std::endian::native>
            {
                if constexpr (std::endian::little == endian)
                    return std::byteswap(size);
                else
                    return size;
            }),
    };
    return {
        .bytes = std::invoke(
            [header, payload]
            {
                const auto headerBytes =
                    std::bit_cast<std::array<char, sizeof(FrameHeader)>>(header);
                return std::string(headerBytes.begin(), headerBytes.end()) + std::string(payload);
            }),
    };
}

struct RtspFrame
{
    std::uint8_t channelId = 0;
    std::string payload;

    bool operator==(const RtspFrame&) const = default;
};

struct RtspReadError
{
    bool operator==(const RtspReadError&) const = default;
};

void PrintTo(const RtspFrame& frame, std::ostream* os)
{
    *os << "RtspFrame{channelId=" << static_cast<int>(frame.channelId)
        << ", payload=" << ::testing::PrintToString(frame.payload) << "}";
}

void PrintTo(const RtspReadError&, std::ostream* os)
{
    *os << "RtspReadError{}";
}

using RtspOutput = std::variant<RtspFrame, RtspReadError>;

struct RtspStreamData
{
    std::string bytes;
    std::size_t readPosition = 0;
};

struct RtspScenario
{
    TracedName name;
    std::vector<RtspStreamMessage> input;
    std::vector<RtspOutput> expected;
};

class RtspBadMessages:
    public ::testing::TestWithParam<RtspScenario>
{
};

class AppendableBufferSocket:
    public nx::network::DummySocket
{
public:
    AppendableBufferSocket(std::shared_ptr<RtspStreamData> streamData):
        m_streamData(std::move(streamData))
    {
    }

    bool close() override
    {
        m_isOpened = false;
        return true;
    }

    bool shutdown() override
    {
        m_isOpened = false;
        return true;
    }

    bool isClosed() const override
    {
        return !m_isOpened;
    }

    bool connect(
        const nx::network::SocketAddress& remoteSocketAddress,
        std::chrono::milliseconds timeout) override
    {
        if (!DummySocket::connect(remoteSocketAddress, timeout))
            return false;

        m_isOpened = true;
        m_streamData->readPosition = 0;
        return true;
    }

    int recv(void* buffer, std::size_t bufferLength, int /*flags*/ = 0) override
    {
        const std::size_t bytesToCopy = std::min(
            bufferLength,
            m_streamData->bytes.size() - m_streamData->readPosition);
        std::memcpy(buffer, m_streamData->bytes.data() + m_streamData->readPosition, bytesToCopy);
        m_streamData->readPosition += bytesToCopy;
        return static_cast<int>(bytesToCopy);
    }

    int send(const void* /*buffer*/, std::size_t bufferLength) override
    {
        return m_isOpened ? static_cast<int>(bufferLength) : -1;
    }

    bool isConnected() const override
    {
        return m_isOpened;
    }

private:
    std::shared_ptr<RtspStreamData> m_streamData;
    bool m_isOpened = false;
};

struct RtspClientState
{
    QnRtspClient* client = nullptr;
    std::shared_ptr<RtspStreamData> streamData;
    std::vector<RtspOutput> outputs;
};

std::vector<RtspOutput> drainAvailableOutputs(QnRtspClient* client)
{
    std::vector<RtspOutput> outputs;

    while (client->isOpened())
    {
        std::vector<nx::utils::ByteArray*> demuxedData;
        // Existing demux API returns owning raw pointers through the output vector.
        const auto deleteDemuxedData = nx::utils::makeScopeGuard(
            [&demuxedData]
            {
                std::ranges::for_each(
                    demuxedData,
                    [](nx::utils::ByteArray* data) { delete data; });
            });

        int channelNumber = -1;
        const int readResult = client->readBinaryResponse(demuxedData, channelNumber);

        if (readResult < 0)
        {
            outputs.emplace_back(RtspReadError{});
            break;
        }

        if (readResult == 0)
            break;

        if (channelNumber >= 0
            && std::cmp_less(channelNumber, demuxedData.size())
            && demuxedData[channelNumber])
        {
            const auto& data = *demuxedData[channelNumber];
            outputs.emplace_back(RtspFrame{
                .channelId = static_cast<std::uint8_t>(channelNumber),
                .payload = data.size() >= sizeof(FrameHeader)
                    ? std::string(
                        data.data() + sizeof(FrameHeader),
                        data.size() - sizeof(FrameHeader))
                    : std::string(),
            });
        }
    }

    return outputs;
}

TEST_P(RtspBadMessages, RecoversFromBadMessages)
{
    constexpr std::string_view kUrl = "rtsp://127.0.0.1/test";

    const RtspScenario& scenario = GetParam();
    const auto trace = scenario.name.trace();

    QnRtspClient rtspClient(QnRtspClient::Config{});
    rtspClient.setPlayNowModeAllowed(true);

    // The client owns the socket, while the test must append input chunks during the fold.
    std::shared_ptr streamData = std::make_shared<RtspStreamData>();
    std::unique_ptr socket = std::invoke(
        [streamData]
        {
            std::unique_ptr socket = std::make_unique<AppendableBufferSocket>(streamData);
            EXPECT_TRUE(socket->connect(
                nx::network::SocketAddress(nx::network::HostAddress::localhost, 554),
                std::chrono::milliseconds::zero()));
            return socket;
        });
    rtspClient.setProxySocket(std::move(socket));
    ASSERT_TRUE(rtspClient.open(kUrl));

    // Treat the RTSP client as a stateful stream transducer:
    //   (client state, input chunk) -> (client state, zero or more observable outputs).
    // AppendableBufferSocket is the wrapper that lets the test feed chunks incrementally while
    // QnRtspClient keeps owning and reading from its socket normally.
    const auto actual = nx::ranges::fold_left(scenario.input,
        RtspClientState{.client = &rtspClient, .streamData = std::move(streamData)},
        [](RtspClientState state, const RtspStreamMessage& message)
        {
            state.streamData->bytes += message.bytes;
            std::ranges::move(drainAvailableOutputs(state.client),
                std::back_inserter(state.outputs));
            return state;
        }).outputs;

    EXPECT_EQ(scenario.expected, actual);
}

const std::vector<RtspScenario> kRtspScenarios{
    {
        .name = TRACED_NAME("ValidFrames"),
        .input = {
            validFrame(""),
            validFrame("ab"),
        },
        .expected =
            {
                RtspFrame{.payload = ""},
                RtspFrame{.payload = "ab"},
            },
    },
    {
        .name = TRACED_NAME("DifferentChannelFrame"),
        .input = {
            validFrame("metadata", /*channelId*/ 2),
        },
        .expected =
            {
                RtspFrame{.channelId = 2, .payload = "metadata"},
            },
    },
    {
        .name = TRACED_NAME("TextMessagesBetweenFrames"),
        .input = {
            {.bytes = "RTSP/1.0 200 OK\r\nCSeq: 1\r\n\r\n"},
            validFrame(""),
            {.bytes = "RTSP/1.0 200 OK\r\nCSeq: 2\r\n\r\n"},
            validFrame("ab"),
        },
        .expected =
            {
                RtspFrame{.payload = ""},
                RtspFrame{.payload = "ab"},
            },
    },
    // FIXME: #skolesnik. Assumed to work, but the current parser emits read errors instead of
    // resynchronizing when garbage appears before the next valid frame header. Re-enable after the
    // recovery behavior is clarified independently from the full-buffer retry bug.
    // {
    //     .name = TRACED_NAME("GarbageAtStreamStartBeforeFrame"),
    //     .input = {
    //         {.bytes = "garbage"},
    //         validFrame(""),
    //     },
    //     .expected =
    //         {
    //             RtspFrame{.payload = ""},
    //         },
    // },
    // {
    //     .name = TRACED_NAME("LargeGarbageStillLeavesSpaceForFrameHeader"),
    //     .input = {
    //         {
    //             .bytes = std::string(
    //                 QnRtspClient::kResponseBufferLength - sizeof(FrameHeader),
    //                 'A'),
    //         },
    //         validFrame(""),
    //     },
    //     .expected =
    //         {
    //             RtspFrame{.payload = ""},
    //         },
    // },
    {
        .name = TRACED_NAME("MalformedTextAfterFrameDoesNotRetrySameBuffer"),
        .input =
            {
                validFrame(""),
                {.bytes = std::string(QnRtspClient::kResponseBufferLength, 'A')},
                validFrame(""),
            },
        .expected =
            {
                RtspFrame{.payload = ""},
                RtspReadError{},
                RtspFrame{.payload = ""},
            },
    },
};

#undef TRACED_NAME

INSTANTIATE_TEST_SUITE_P(
    Rtsp,
    RtspBadMessages,
    ::testing::ValuesIn(kRtspScenarios),
    /*name_generator*/ [](const auto& info) { return std::string(info.param.name()); });

} // namespace
