// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <deque>
#include <fstream>

#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/webrtc/webrtc_dtls.h>

using namespace nx::webrtc;

using Queue = std::deque<std::vector<uint8_t>>;

class WebRtcDtlsStreamer: public AbstractDtlsDelegate
{
public:
    WebRtcDtlsStreamer(const std::string& id, bool client);
    virtual ~WebRtcDtlsStreamer() = default;
    virtual void writeDtlsPacket(const char* data, int size) override final;
    virtual bool onDtlsData(const uint8_t* data, int size) override final;

    // Test API.
    bool writePacket(const char* data, int size);
    Dtls::Status handlePacket(const char* data, int size);
    Queue outputQueue();
    Queue inputQueue();
    Fingerprint fingerprint() const;
    void setFingerprint(const Fingerprint& fingerprint) { m_dtls->setFingerprint(fingerprint); }
private:
    std::unique_ptr<Dtls> m_dtls;
    nx::network::ssl::Pem m_pem;
    Queue m_output;
    Queue m_input;
};

WebRtcDtlsStreamer::WebRtcDtlsStreamer(const std::string& id, bool client)
{
    const auto pemString =
        nx::network::ssl::makeCertificateAndKey({"Common Name", "US", "Organization"});
    EXPECT_TRUE(m_pem.parse(pemString));
    m_dtls = std::make_unique<Dtls>(id, m_pem, client ? Dtls::Type::client : Dtls::Type::server);
}

Fingerprint WebRtcDtlsStreamer::fingerprint() const
{
    Fingerprint fingerprint;
    fingerprint.type = Fingerprint::Type::sha256;
    fingerprint.hash = nx::network::ssl::CertificateView(m_pem.certificate().x509()).sha256();
    return fingerprint;
}

void WebRtcDtlsStreamer::writeDtlsPacket(const char* data, int size)
{
    m_output.emplace_back((const uint8_t*) data, (const uint8_t*) &data[size]);
}

bool WebRtcDtlsStreamer::onDtlsData(const uint8_t* data, int size)
{
    m_input.emplace_back((const uint8_t*) data, (const uint8_t*) &data[size]);
    return true;
}

bool WebRtcDtlsStreamer::writePacket(const char* data, int size)
{
    return Dtls::Status::error != m_dtls->writePacket((const uint8_t*) data, size, this);
}

Dtls::Status WebRtcDtlsStreamer::handlePacket(const char* data, int size)
{
    return m_dtls->handlePacket((const uint8_t*) data, size, this);
}

Queue WebRtcDtlsStreamer::outputQueue()
{
    return std::exchange(m_output, {});
}

Queue WebRtcDtlsStreamer::inputQueue()
{
    return std::exchange(m_input, {});
}

class WebRtcDtls: public ::testing::Test
{
public:
    void fingerprintExchange();
    void fingerprintExchangeBroken();
    bool handshake(int mtu = 1232);
    bool writeToServerAndCheck(const Queue& input);
    bool writeToClientAndCheck(const Queue& input);

private:
    WebRtcDtlsStreamer m_dtlsClient{"client-id", true};
    WebRtcDtlsStreamer m_dtlsServer{"server-id", false};
};

void WebRtcDtls::fingerprintExchange()
{
    m_dtlsClient.setFingerprint(m_dtlsServer.fingerprint());
    m_dtlsServer.setFingerprint(m_dtlsClient.fingerprint());
}

void WebRtcDtls::fingerprintExchangeBroken()
{
    m_dtlsClient.setFingerprint(m_dtlsClient.fingerprint());
    m_dtlsServer.setFingerprint(m_dtlsServer.fingerprint());
}

Dtls::Status performIo(WebRtcDtlsStreamer* streamer, Queue& queue)
{
    Dtls::Status status = Dtls::Status::initialized;
    while (!queue.empty())
    {
        auto data = std::move(queue.front());
        queue.pop_front();
        status = streamer->handlePacket((const char*) data.data(), data.size());
        if (status == Dtls::Status::error)
            return status;
    }
    queue = streamer->outputQueue();
    return status;
}

bool performReadWrite(
    WebRtcDtlsStreamer* src,
    WebRtcDtlsStreamer* dst,
    const Queue& input,
    Queue& output)
{
    for (const auto& i: input)
    {
        bool ret = src->writePacket((const char*) i.data(), i.size());
        EXPECT_TRUE(ret);
        if (!ret)
            return false;
        Queue srcOutput = src->outputQueue();
        auto status = performIo(dst, srcOutput);
        EXPECT_EQ(status, Dtls::Status::streaming);
        if (status != Dtls::Status::streaming)
            return false;
        EXPECT_TRUE(srcOutput.empty());
        if (!srcOutput.empty())
            return false;
        Queue dstOutput = dst->inputQueue();
        for (const auto& j: dstOutput)
            output.push_back(j);
    }
    return true;
}

bool WebRtcDtls::handshake(int mtu)
{
    Queue queue;
    queue.emplace_back(std::vector<uint8_t>()); //< Start handshake with empty data input into client.
    std::array<WebRtcDtlsStreamer*, 2> streamers{ &m_dtlsClient, &m_dtlsServer };
    int successCount = 0;

    for (int i = 0; i < 8; ++i)
    {
        for (const auto& j: queue)
        {
            if (j.size() > (size_t) mtu)
                return false;
        }
        auto status = performIo(streamers[i % 2], queue);
        if (status == Dtls::Status::error)
            return false;
        EXPECT_TRUE(status != Dtls::Status::initialized);
        if (status == Dtls::Status::streaming)
            ++successCount;
        if (successCount == 2)
            return true;
    }
    return false;
}

bool WebRtcDtls::writeToServerAndCheck(const Queue& input)
{
    Queue output;
    bool ret = performReadWrite(&m_dtlsClient, &m_dtlsServer, input, output);
    EXPECT_TRUE(ret);
    if (!ret)
        return false;
    EXPECT_EQ(input, output);
    return input == output;
}

bool WebRtcDtls::writeToClientAndCheck(const Queue& input)
{
    Queue output;
    bool ret = performReadWrite(&m_dtlsServer, &m_dtlsClient, input, output);
    EXPECT_TRUE(ret);
    if (!ret)
        return false;
    EXPECT_EQ(input, output);
    return input == output;
}

template<typename... Args>
static Queue makeInput(Args... vectors)
{
    Queue input;
    input.emplace_back(vectors...);
    return input;
}

TEST_F(WebRtcDtls, DtlsSimpleTest)
{
    fingerprintExchange();
    EXPECT_TRUE(handshake());

    EXPECT_TRUE(writeToClientAndCheck(makeInput(3, 0x42)));
    EXPECT_TRUE(writeToServerAndCheck(makeInput(3, 0x43)));
}

TEST_F(WebRtcDtls, DtlsSeveralBigPackets)
{
    fingerprintExchange();
    EXPECT_TRUE(handshake());

    EXPECT_TRUE(writeToClientAndCheck(makeInput(4096, 0x42)));
    {
        Queue input;
        input.emplace_back(1024, 0x41);
        input.emplace_back(4096, 0x42);
        input.emplace_back(1024, 0x43);
        EXPECT_TRUE(writeToServerAndCheck(input));
    }
}

TEST_F(WebRtcDtls, DtlsBadMtu)
{
    fingerprintExchange();
    EXPECT_FALSE(handshake(1231)); //< Can set DTLS MTU only for handshake stage.
}

TEST_F(WebRtcDtls, DtlsBadFingerprint)
{
    fingerprintExchangeBroken();
    EXPECT_FALSE(handshake());
}
