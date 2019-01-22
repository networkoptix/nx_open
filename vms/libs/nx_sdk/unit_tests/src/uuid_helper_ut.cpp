#include <nx/kit/test.h>

#include <map>
#include <string>
#include <sstream>
#include <cctype>

#include <nx/sdk/helpers/uuid_helper.h>

namespace nx {
namespace sdk {
namespace test {

#define FIXED_UUID {0xd4,0xbb,0x55,0xa4,0xa8,0x7a,0x41,0x99,0xbb,0xd1,0x80,0x00,0xa4,0x80,0xa5,0xad}
static constexpr char kFixedUuidString[]{"{D4BB55A4-A87A-4199-BBD1-8000A480A5AD}"};
static constexpr uint8_t kFixedUuidBytes[Uuid::kSize] FIXED_UUID;
static constexpr Uuid kFixedUuid FIXED_UUID;

static constexpr char kNullUuidString[]{"{00000000-0000-0000-0000-000000000000}"};
static constexpr uint8_t kNullUuidBytes[Uuid::kSize]{};
static constexpr Uuid kNullUuid = Uuid(); //< default constructor

TEST(UuidHelper, uuidBasics)
{
    ASSERT_FALSE(kFixedUuid.isNull());

    ASSERT_EQ(kFixedUuid, kFixedUuid); //< operator==() for identical arguments
    ASSERT_FALSE(kFixedUuid != kFixedUuid); //< operator!=() for identical arguments

    const Uuid sameFixedUuid{kFixedUuid}; //< copy-constructor
    ASSERT_EQ(kFixedUuid, sameFixedUuid); //< operator==()

    Uuid anotherSameFixedUuid; //< default constructor
    ASSERT_EQ(kNullUuid, anotherSameFixedUuid); //< operator==()
    anotherSameFixedUuid = kFixedUuid; //< operator=()
    ASSERT_EQ(kFixedUuid, anotherSameFixedUuid); //< operator==()

    ASSERT_TRUE(kFixedUuid != kNullUuid); //< operator!=() for different uuids
    ASSERT_FALSE(kFixedUuid == kNullUuid); //< operator==() for different uuids

    // Test that the default constructor yields an all-zeros uuid.
    constexpr Uuid kAllZerosUuid{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    ASSERT_TRUE(kAllZerosUuid.isNull());
    constexpr Uuid kDefaultUuid = Uuid(); //< default constructor
    ASSERT_TRUE(kDefaultUuid.isNull());
    ASSERT_EQ(kAllZerosUuid, kDefaultUuid); //< operator==()

    ASSERT_EQ(kNullUuid, kDefaultUuid); //< Check the integrity of this test.

    for (const uint8_t b: kNullUuid)
        ASSERT_EQ(0, b);

    for (int i = 0; i < Uuid::kSize; ++i)
        ASSERT_EQ(kFixedUuidBytes[i], kFixedUuid[i]);
}

TEST(UuidHelper, uuidRandom)
{
    const Uuid random1 = UuidHelper::randomUuid();
    const Uuid random2 = UuidHelper::randomUuid();
    ASSERT_FALSE(random1.isNull());
    ASSERT_FALSE(random2.isNull());
    ASSERT_FALSE(random1 == random2);
}

TEST(UuidHelper, uuidBinaryCompatibilityWithOldSdk)
{
    ASSERT_EQ(16, (int) sizeof(Uuid));
    ASSERT_EQ((int) sizeof(Uuid), Uuid::kSize);
    ASSERT_EQ(sizeof(Uuid), sizeof(kFixedUuidBytes)); //< Check the integrity of this test.
    ASSERT_EQ(0, memcmp(&kFixedUuid, kFixedUuidBytes, sizeof(kFixedUuidBytes)));
    ASSERT_EQ(0, memcmp(&kNullUuid, kNullUuidBytes, sizeof(kNullUuidBytes)));
}

static void testUuidToStdString(const std::string& expectedString, const Uuid& uuid)
{
    std::ostringstream stream;
    stream << uuid; //< operator<<()
    ASSERT_STREQ(expectedString, stream.str());

    ASSERT_STREQ(expectedString, UuidHelper::toStdString(uuid));
}

TEST(UuidHelper, toStdString)
{
    testUuidToStdString(kNullUuidString, kNullUuid);
    testUuidToStdString(kFixedUuidString, kFixedUuid);

    std::string fixedUuidWithoutFormatOptions;
    for (const char c: kFixedUuidString)
    {
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            fixedUuidWithoutFormatOptions.push_back(tolower(c));
    }
    ASSERT_STREQ(fixedUuidWithoutFormatOptions,
        UuidHelper::toStdString(kFixedUuid, UuidHelper::FormatOptions::none));
}

TEST(UuidHelper, fromRawData)
{
    const Uuid uuidFromRawData = UuidHelper::fromRawData(kFixedUuidBytes);
    ASSERT_EQ(kFixedUuid, uuidFromRawData); //< operator==()
    for (int i = 0; i < Uuid::kSize; ++i)
        ASSERT_EQ(kFixedUuidBytes[i], uuidFromRawData[i]);
}

TEST(UuidHelper, fromStdString)
{
    const std::map<std::string, Uuid> goodUuids{
        {
            kNullUuidString,
            kNullUuid
        },
        {
            kFixedUuidString,
            kFixedUuid
        },
        {
            "{b6565a87-5897-43b4-8f1a-ea1778a65ed1}",
            {0xb6,0x56,0x5a,0x87,0x58,0x97,0x43,0xb4,0x8f,0x1a,0xea,0x17,0x78,0xa6,0x5e,0xd1}
        },
        {
            "1be1f39b-96a6-481a-aa63-b4886314ad65",
            {0x1b,0xe1,0xf3,0x9b,0x96,0xa6,0x48,0x1a,0xaa,0x63,0xb4,0x88,0x63,0x14,0xad,0x65}
        },
        {
            "4b47b06f76fd4f76b58df88a303de631",
            {0x4b,0x47,0xb0,0x6f,0x76,0xfd,0x4f,0x76,0xb5,0x8d,0xf8,0x8a,0x30,0x3d,0xe6,0x31}
        },
        {
            "D7BFE5822B844E26AAAA40DA5CA5F097",
            {0xd7,0xbf,0xe5,0x82,0x2b,0x84,0x4e,0x26,0xaa,0xaa,0x40,0xda,0x5c,0xa5,0xf0,0x97}
        },
        {
            "C9560F62-EC0D-45E9-B2A5-190A9F76B778",
            {0xc9,0x56,0x0f,0x62,0xec,0x0d,0x45,0xe9,0xb2,0xa5,0x19,0x0a,0x9f,0x76,0xb7,0x78}
        },
        {
            "   {D3BB55A4-A87A-4199-BBD1-8000A4  80A5AD}   ",
            {0xd3,0xbb,0x55,0xa4,0xa8,0x7a,0x41,0x99,0xbb,0xd1,0x80,0x00,0xa4,0x80,0xa5,0xad}
        },
    };

    const std::vector<std::string> badUuidStrings{
        "{asdadsjhgjhg",
        "C9560F62-EC0D-45E9-B2A5-190A9F76B778adadae",
        "D7BFE5822B844E26AAAA40DA5CA5R097",
        "<1be1f39b-96a6-481a-aa63-b4886314ad65>"
    };

    for (const auto& entry: goodUuids)
        ASSERT_EQ(UuidHelper::fromStdString(entry.first), entry.second);

    for (const auto& uuidString: badUuidStrings)
        ASSERT_EQ(UuidHelper::fromStdString(uuidString), kNullUuid);
}

} // namespace test
} // namespace sdk
} // namespace nx
