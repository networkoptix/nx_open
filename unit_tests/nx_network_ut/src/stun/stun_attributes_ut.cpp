#include <iostream>

#include <gtest/gtest.h>

#include <nx/network/stun/stun_attributes.h>
#include <nx/network/stun/message_parser.h>

namespace nx {
namespace network {
namespace stun {
namespace attrs {
namespace test {

//-------------------------------------------------------------------------------------------------
// StunAttributeCommonTest

template<typename Attribute>
class StunAttributeCommonTest:
    public ::testing::Test
{
protected:
    void addTestValue(
        const nx::Buffer& serializedRepresentation,
        const Attribute& attributeValue)
    {
        m_testValues.push_back({serializedRepresentation, attributeValue});
    }

    void runTests()
    {
        ASSERT_FALSE(m_testValues.empty());

        serializationTest();
        deserializationTest();
        serializationReversibleTest();
    }

private:
    struct TestDataContext
    {
        const nx::Buffer serializedValue;
        const Attribute value;
    };

    std::list<TestDataContext> m_testValues;

    void serializationTest()
    {
        std::cout << "[  SUBTEST ] serialization" << std::endl;

        for (const auto& testData: m_testValues)
        {
            const nx::Buffer& serializedPrototype = testData.serializedValue;
            Attribute testAttribute = testData.value;

            nx::Buffer serializedBuffer;
            serializedBuffer.reserve(serializedPrototype.size());
            std::size_t bytesWritten = 0;
            MessageSerializerBuffer stream(&serializedBuffer);
            ASSERT_EQ(
                nx::network::server::SerializerState::done,
                testAttribute.serialize(&stream, &bytesWritten));
            ASSERT_EQ((size_t)serializedPrototype.size(), bytesWritten);
            ASSERT_EQ(serializedPrototype, serializedBuffer);
        }
    }

    void deserializationTest()
    {
        std::cout << "[  SUBTEST ] deserialization" << std::endl;

        for (const auto& testData: m_testValues)
        {
            const nx::Buffer& serializedPrototype = testData.serializedValue;

            MessageParserBuffer stream(serializedPrototype);
            Attribute testAttribute;
            ASSERT_TRUE(testAttribute.deserialize(&stream));
            ASSERT_EQ(testData.value, testAttribute);
        }
    }

    void serializationReversibleTest()
    {
        std::cout << "[  SUBTEST ] serializationReversible" << std::endl;

        const Attribute originalAttribute = m_testValues.front().value;
        nx::Buffer buffer;
        {
            buffer.reserve(1);
            MessageSerializerBuffer serializationStream(&buffer);
            std::size_t bytesWritten = 0;
            while (originalAttribute.serialize(&serializationStream, &bytesWritten)
                    != nx::network::server::SerializerState::done)
            {
                buffer.resize(0);
                buffer.reserve(buffer.capacity() * 2);
            }
        }

        Attribute deserializedAttribute;
        {
            MessageParserBuffer parserStream(buffer);
            ASSERT_TRUE(deserializedAttribute.deserialize(&parserStream));
        }
        ASSERT_EQ(originalAttribute, deserializedAttribute);
    }
};

//-------------------------------------------------------------------------------------------------
// StunAttributeMappedAddress

static const uint8_t serializedMappedAddress[] = {
    0, //< Reserved zero
    0x01, //< Family
    0x10, 0x23, //< Port
    0x12, 0x95, 0xA1, 0x0B }; //< Address

class StunAttributeMappedAddress:
    public StunAttributeCommonTest<MappedAddress>
{
public:
    static constexpr const uint16_t portBigEndian = 0x1023;
    static constexpr const uint32_t addressBigEndian = 0x1295A10B;

    StunAttributeMappedAddress()
    {
        in_addr addr;
        memset(&addr, 0, sizeof(addr));
        addr.s_addr = htonl(addressBigEndian);

        SocketAddress endpoint(HostAddress(addr), portBigEndian);
        MappedAddress attribute(std::move(endpoint));

        addTestValue(
            nx::Buffer(
                reinterpret_cast<const char*>(serializedMappedAddress),
                sizeof(serializedMappedAddress)),
            attribute);
    }
};

TEST_F(StunAttributeMappedAddress, composite_test)
{
    runTests();
}

} // namespace test
} // namespace attrs
} // namespace stun
} // namespace network
} // namespace nx
