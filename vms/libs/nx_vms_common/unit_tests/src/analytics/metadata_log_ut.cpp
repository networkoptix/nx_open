// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <limits>

#include <QtCore/QString>

#include <nx/utils/log/log.h>
#include <nx/utils/test_support/test_options.h>

#include <nx/analytics/metadata_logger.h>
#include <nx/analytics/metadata_log_parser.h>
#include <analytics/common/object_metadata.h>

using namespace nx::analytics;
using namespace nx::common::metadata;

using ObjectMetadataPackets = std::vector<ObjectMetadataPacket>;

static void testRange(
    const MetadataLogParser& metadataLogParser,
    int64_t lowerTimestampMs,
    int64_t upperTimestampMs,
    const std::vector<int64_t>& expectedTimestampsMs)
{
    const std::vector<const MetadataLogParser::Packet*> packets =
        metadataLogParser.packetsBetweenTimestamps(lowerTimestampMs, upperTimestampMs);

    const auto details =
        [&expectedTimestampsMs, &packets]()
        {
            QString result = "  Details:";

            QString expected;
            for (int i = 0; i < (int) expectedTimestampsMs.size(); ++i)
                expected += ((i > 0) ? ", " : "") + QString::number(expectedTimestampsMs[i]);
            result += "\n    Expected packet timestamps: {" + expected + "}";

            QString actual;
            for (int i = 0; i < (int) packets.size(); ++i)
                actual += ((i > 0) ? ", " : "") + QString::number(packets[i]->timestampMs);
            result += "\n    Actual packet timestamps: {" + actual + "}";

            return result.toStdString();
        };

    ASSERT_EQ(expectedTimestampsMs.size(), packets.size()) << details();

    for (int i = 0; i < (int) expectedTimestampsMs.size(); ++i)
        ASSERT_EQ(expectedTimestampsMs[i], packets[i]->timestampMs) << details();
}
#define TEST_RANGE(...) ASSERT_NO_FATAL_FAILURE(testRange(__VA_ARGS__))

static ObjectMetadata makeObjectMetadata(float x, float y, float width, float height)
{
    ObjectMetadata objectMetadata;
    objectMetadata.boundingBox = QRectF(x, y, width, height);
    return objectMetadata;
}

static ObjectMetadataPacket makeObjectMetadataPacket(
    int64_t timestampMs,
    std::vector<ObjectMetadata> objectMetadataList)
{
    ObjectMetadataPacket objectMetadataPacket;
    objectMetadataPacket.timestampUs = timestampMs * 1000;
    objectMetadataPacket.objectMetadataList = std::move(objectMetadataList);
    return objectMetadataPacket;
}

static ObjectMetadataPackets makeObjectMetadataPackets()
{
    ObjectMetadataPackets result;

    result.push_back(makeObjectMetadataPacket(10, {
        makeObjectMetadata(1.0F, 1.0F, 0.0F, 0.0F),
        makeObjectMetadata(0.271F, 0.314F, 0.5F, 0.6F),
    }));
    result.push_back(makeObjectMetadataPacket(20, {
        makeObjectMetadata(0.1F, 0.2F, 0.3F, 0.4F),
    }));
    result.push_back(makeObjectMetadataPacket(30, {}));
    result.push_back(makeObjectMetadataPacket(40, {}));
    result.push_back(makeObjectMetadataPacket(50, {}));
    result.push_back(makeObjectMetadataPacket(60, {}));

    return result;
}

/** Test MetadataLogParser::packetsBetweenTimestamps(). */
static void testRanges(const MetadataLogParser& metadataLogParser)
{
    // Misses.
    TEST_RANGE(metadataLogParser, 100, 105, {}); //< Totally higher.
    TEST_RANGE(metadataLogParser, 60, 100, {}); //< Highest packet excluded.

    TEST_RANGE(metadataLogParser, 0, 0, {});
    TEST_RANGE(metadataLogParser, 20, 20, {}); //< Empty range.
    TEST_RANGE(metadataLogParser, 1, 5, {}); //< Totally lower.

    // Hits.
    TEST_RANGE(metadataLogParser, 0, 100, {10, 20, 30, 40, 50, 60}); //< Full range.
    TEST_RANGE(metadataLogParser, 0, 10, {10}); //< Lowest packet included.
    TEST_RANGE(metadataLogParser, 20, 50, {30, 40, 50});

    // Non-exact matches.
    TEST_RANGE(metadataLogParser, 25, 45, {30, 40});
    TEST_RANGE(metadataLogParser, 20, 45, {30, 40});
    TEST_RANGE(metadataLogParser, 25, 50, {30, 40, 50});
}
#define TEST_RANGES(...) ASSERT_NO_FATAL_FAILURE(testRanges(__VA_ARGS__))

static void testEqualRects(
    const ObjectMetadataPackets& objectMetadataPackets,
    const MetadataLogParser& metadataLogParser)
{
    const std::vector<const MetadataLogParser::Packet*> packets =
        metadataLogParser.packetsBetweenTimestamps(
            std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());

    ASSERT_EQ(objectMetadataPackets.size(), packets.size());
    for (const auto& objectMetadataPacket: objectMetadataPackets)
    {
        const auto packet = std::find_if(packets.cbegin(), packets.cend(),
            [&objectMetadataPacket](
                const MetadataLogParser::Packet* packet)
            {
                return packet->timestampMs * 1000 == objectMetadataPacket.timestampUs;
            });
        ASSERT_TRUE(packet != packets.cend());
        ASSERT_EQ(objectMetadataPacket.timestampUs, (*packet)->timestampMs * 1000);

        const auto& rects = (*packet)->rects;
        ASSERT_EQ(objectMetadataPacket.objectMetadataList.size(), rects.size());
        for (int i = 0; i < (int) rects.size(); ++i)
            ASSERT_EQ(rects[i], objectMetadataPacket.objectMetadataList[i].boundingBox);
    }
}
#define TEST_EQUAL_RECTS(...) ASSERT_NO_FATAL_FAILURE(testEqualRects(__VA_ARGS__))

TEST(MetadataLog, metadataLoggerAndLogParser)
{
    const QString testDir = nx::TestOptions::temporaryDirectoryPath(/*canCreate*/ true);
    const QString kMetadataLoggerFilename = testDir + "/metadata_logger.log";
    const QString kMetadataLogParserFilename = testDir + "/metadata_log_parser.log";

    const auto& objectMetadataPackets = makeObjectMetadataPackets();

    {
        NX_INFO(this, "Writing file using MetadataLogger: %1", kMetadataLoggerFilename);
        MetadataLogger metadataLogger(kMetadataLoggerFilename);
        for (const auto& objectMetadataPacket: objectMetadataPackets)
            metadataLogger.pushObjectMetadata(objectMetadataPacket);
    }

    {
        NX_INFO(this, "Parsing file written by MetadataLogger using MetadataLogParser");
        MetadataLogParser metadataLogParser;
        ASSERT_TRUE(metadataLogParser.loadLogFile(kMetadataLoggerFilename));
        TEST_EQUAL_RECTS(objectMetadataPackets, metadataLogParser);

        TEST_RANGES(metadataLogParser);

        NX_INFO(this, "Writing file using MetadataLogParser: %1", kMetadataLogParserFilename);
        ASSERT_TRUE(metadataLogParser.saveToFile(kMetadataLogParserFilename));
    }

    {
        NX_INFO(this, "Parsing file written by MetadataLogParser using MetadataLogParser");
        MetadataLogParser metadataLogParser;
        ASSERT_TRUE(metadataLogParser.loadLogFile(kMetadataLogParserFilename));
        TEST_EQUAL_RECTS(objectMetadataPackets, metadataLogParser);
    }
}
