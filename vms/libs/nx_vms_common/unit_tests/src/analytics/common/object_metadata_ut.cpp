// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <analytics/common/object_metadata.h>

namespace nx::common::metadata::test {

TEST(metadataAttributes, twoGroups)
{
    Attributes attributes;
    addAttributeIfNotExists(&attributes, {"name1","value1"});
    addAttributeIfNotExists(&attributes, {"name1","value2"});
    addAttributeIfNotExists(&attributes, {"name2","value1"});
    addAttributeIfNotExists(&attributes, {"name1","value3"});

    const auto result = groupAttributes(attributes);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ(result[0].name, "name1");
    ASSERT_EQ(result[0].values, QStringList({"value1", "value2", "value3"}));
    ASSERT_EQ(result[1].name, "name2");
    ASSERT_EQ(result[1].values, QStringList({"value1"}));
}

TEST(metadataAttributes, oneGroup)
{
    Attributes attributes;
    addAttributeIfNotExists(&attributes, {"name1", "value1"});
    addAttributeIfNotExists(&attributes, {"name1", "value2"});

    const auto result = groupAttributes(attributes);
    ASSERT_EQ(1, result.size());
    ASSERT_EQ(result[0].name, "name1");
    ASSERT_EQ(result[0].values, QStringList({"value1", "value2"}));
}

TEST(metadataAttributes, simpleGroups)
{
    Attributes attributes;
    addAttributeIfNotExists(&attributes, {"name1", "value1"});
    addAttributeIfNotExists(&attributes, {"name2", "value2"});

    const auto result = groupAttributes(attributes);
    ASSERT_EQ(2, result.size());
    ASSERT_EQ(result[0].name, "name1");
    ASSERT_EQ(result[0].values, QStringList({"value1"}));
    ASSERT_EQ(result[1].name, "name2");
    ASSERT_EQ(result[1].values, QStringList({"value2"}));
}

TEST(metadataAttributes, filterOutHidden)
{
    Attributes attributes;
    addAttributeIfNotExists(&attributes, {"nx.sys.testAttribute", "value1"});
    addAttributeIfNotExists(&attributes, {"nx.sys.testAttribute", "value2"});

    const auto filtered = groupAttributes(attributes); //< Filters hidden attributes out by default.
    ASSERT_EQ(0, filtered.size());

    const auto unfiltered = groupAttributes(
        attributes, /*humanReadableNames*/ false, /*filterOutHidden*/ false);
    ASSERT_EQ(1, unfiltered.size());
    ASSERT_EQ(unfiltered[0].name, "nx.sys.testAttribute");
    ASSERT_EQ(unfiltered[0].values, QStringList({"value1", "value2"}));
}

TEST(metadataAttributes, humanReadableNames)
{
    Attributes attributes;
    addAttributeIfNotExists(&attributes, {"Parent Name.Nested Name", "TRUE"});
    addAttributeIfNotExists(&attributes, {"Parent Name.Nested Name", "fAlSe"});
    addAttributeIfNotExists(&attributes, {"Parent Name.Nested Name", "true story"});

    const auto humanReadable = groupAttributes(attributes);
    ASSERT_EQ(1, humanReadable.size());
    ASSERT_EQ(humanReadable[0].name, "Parent Name.Nested Name");
    ASSERT_EQ(humanReadable[0].values, QStringList({"True", "False", "true story"}));

    const auto sourceNames = groupAttributes(attributes, /*humanReadableNames*/ false);
    ASSERT_EQ(1, sourceNames.size());
    ASSERT_EQ(sourceNames[0].name, "Parent Name.Nested Name");
    ASSERT_EQ(sourceNames[0].values, QStringList({"TRUE", "fAlSe", "true story" }));
}

TEST(QnCompressedObjectMetadataPacket, Clone)
{
    const auto data =
        std::make_shared<QnCompressedObjectMetadataPacket>(MetadataType::ObjectDetection);
    data->channelNumber = 42;
    data->compressionType = AV_CODEC_ID_H265;
    data->context = std::make_shared<CodecParameters>();
    data->dataProvider = (QnAbstractStreamDataProvider*) 0x42;
    data->dataType = QnAbstractMediaData::GENERIC_METADATA;
    data->flags = QnAbstractMediaData::MediaFlags_AVKey;
    data->encryptionData = std::vector<uint8_t>{42};
    data->metadataType = MetadataType::ObjectDetection;
    data->packet = std::make_shared<ObjectMetadataPacket>();
    data->timestamp = 1042;

    const auto clonedMediaData = QnAbstractMediaDataPtr(data->clone());
    const auto clonedMetadata =
        dynamic_cast<const QnCompressedObjectMetadataPacket*>(clonedMediaData.get());

    ASSERT_TRUE(clonedMetadata);

    ASSERT_EQ(data->channelNumber, clonedMetadata->channelNumber);
    ASSERT_EQ(data->compressionType, clonedMetadata->compressionType);
    ASSERT_EQ(data->context, clonedMetadata->context);
    ASSERT_EQ(data->dataProvider, clonedMetadata->dataProvider);
    ASSERT_EQ(data->flags, clonedMetadata->flags);
    ASSERT_EQ(data->encryptionData, clonedMetadata->encryptionData);
    ASSERT_EQ(data->metadataType, clonedMetadata->metadataType);
    ASSERT_EQ(data->packet, clonedMetadata->packet);
    ASSERT_EQ(data->timestamp, clonedMetadata->timestamp);
}

} // namespace
