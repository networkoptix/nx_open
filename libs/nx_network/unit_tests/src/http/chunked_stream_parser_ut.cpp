#include <random>

#include <gtest/gtest.h>

#include <nx/network/http/chunked_stream_parser.h>
#include <nx/network/http/chunked_transfer_encoder.h>
#include <nx/utils/random.h>

namespace nx::network::http::test {

class HttpChunkedStreamParser:
    public ::testing::Test
{
protected:
    void generateRandomChunkedBody(bool addExtensions = false)
    {
        constexpr int maxChunkCount = 3;
        constexpr int maxChunkSize = 10;
        constexpr int maxExtensionCount = 3;
        constexpr int maxExtensionTokenLength = 17;

        // Using random_seed to make test reproducible.
        std::mt19937 randomDevice(::testing::UnitTest::GetInstance()->random_seed());
        std::uniform_int_distribution<> generator(0, maxChunkSize);

        const auto chunkCount = 1 + (generator(randomDevice) % maxChunkCount);
        for (int i = 0; i < chunkCount; ++i)
        {
            auto chunk = nx::utils::random::generate(1 + generator(randomDevice));
            m_body += chunk;

            std::vector<nx::network::http::ChunkExtension> extensions;
            if (addExtensions)
            {
                extensions.resize(generator(randomDevice) % maxExtensionCount);
                for (auto& [name, value]: extensions)
                {
                    name = nx::utils::random::generateName(
                        generator(randomDevice) % maxExtensionTokenLength);
                    value = nx::utils::random::generateName(
                        generator(randomDevice) % maxExtensionTokenLength);
                }
            }

            m_encodedBody += QnChunkedTransferEncoder::serializeSingleChunk(chunk);
        }

        // Adding last chunk.
        m_encodedBody += QnChunkedTransferEncoder::serializeSingleChunk(nx::Buffer());
    }

    void generateRandomChunkedBodyWithExtensions()
    {
        generateRandomChunkedBody(/*addExtensions*/ true);
    }

    void assertBodyIsParsedSuccessfully()
    {
        for (int fragmentSize = 1; fragmentSize < m_encodedBody.size() / 2; fragmentSize <<= 1)
        {
            parseFragmentedBody(fragmentSize);
            assertParsingSucceeded();
        }

        parseFragmentedBody(m_encodedBody.size());
        assertParsingSucceeded();
    }

private:
    nx::Buffer m_body;
    nx::Buffer m_encodedBody;
    nx::Buffer m_parsedBody;
    std::size_t m_bytesParsedCount = 0;
    ChunkedStreamParser m_parser;

private:
    void parseFragmentedBody(int fragmentSize)
    {
        m_parsedBody.clear();
        m_bytesParsedCount = 0;
        m_parser = ChunkedStreamParser();

        for (int offset = 0; offset < m_encodedBody.size();)
        {
            const auto bytesParsed = m_parser.parse(
                QnByteArrayConstRef(
                    m_encodedBody,
                    offset,
                    std::min<>(fragmentSize, m_encodedBody.size()-offset)),
                [this](auto bodyBuf) { m_parsedBody += bodyBuf; });

            offset += bytesParsed;
            m_bytesParsedCount += bytesParsed;
        }
    }

    void assertParsingSucceeded()
    {
        ASSERT_TRUE(m_parser.eof());
        ASSERT_EQ(m_encodedBody.size(), m_bytesParsedCount);
        ASSERT_EQ(m_parsedBody, m_body);
    }
};

TEST_F(HttpChunkedStreamParser, parse_body_without_extensions)
{
    generateRandomChunkedBody();
    assertBodyIsParsedSuccessfully();
}

TEST_F(HttpChunkedStreamParser, parse_body_with_extensions)
{
    generateRandomChunkedBodyWithExtensions();
    assertBodyIsParsedSuccessfully();
}

} // namespace nx::network::http::test
