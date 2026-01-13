// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QtGui/QPainter>

#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/core/transcoding/filters/object_info_filter.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/media/meta_data_packet.h>

class TaxonomyStateWatcher: public nx::analytics::taxonomy::AbstractStateWatcher
{
public:
    virtual std::shared_ptr<nx::analytics::taxonomy::AbstractState> state() const override
    {
        return m_state;
    }

    void setState(std::shared_ptr<nx::analytics::taxonomy::AbstractState> state)
    {
        m_state = state;
        emit stateChanged();
    }

private:
    std::shared_ptr<nx::analytics::taxonomy::AbstractState> m_state;
};

class ObjectInfoFilterTest: public ::testing::Test
{
protected:
    ObjectInfoFilterTest()
    {
        testMap["car"] = {0xff0000};
        testMap["animal"] = {0x0000ff};

        nonNormalizedMetadata = {.typeId = "car"};
        nonNormalizedMetadata.trackId = nx::Uuid();
        nonNormalizedMetadata.boundingBox = {100, 100, 10, 15};

        negativeBoundingBoxMetadata = {.typeId = "car"};
        negativeBoundingBoxMetadata.trackId = nx::Uuid();
        negativeBoundingBoxMetadata.boundingBox = {-1, -1, 1, 1};

        emptyBoundingBoxMetadata = {.typeId = "car"};
        emptyBoundingBoxMetadata.trackId = nx::Uuid();
        emptyBoundingBoxMetadata.boundingBox = QRectF();

        correctAnimalMetadata = {.typeId = "animal"};
        correctAnimalMetadata.trackId = nx::Uuid();
        correctAnimalMetadata.boundingBox = {0.1, 0.1, 0.1, 0.1};

        coloredCarMetadata = {.typeId = "car"};
        coloredCarMetadata.trackId = nx::Uuid();
        coloredCarMetadata.boundingBox = {0.1, 0.1, 0.1, 0.1};
        coloredCarMetadata.attributes.emplace_back("nx.sys.color", "yellow");
    }
    virtual void SetUp() override {}

    std::list<QnConstAbstractCompressedMetadataPtr> generateMetadataList(
        const nx::common::metadata::ObjectMetadataPacket& packet)
    {
        QnCompressedMetadata metadata(MetadataType::ObjectDetection);
        metadata.setData(QnUbjson::serialized<nx::common::metadata::ObjectMetadataPacket>(packet));

        std::list<QnConstAbstractCompressedMetadataPtr> objectMetadataList;
        objectMetadataList.push_back(std::make_shared<const QnCompressedMetadata>(metadata));

        return objectMetadataList;
    }

    nx::core::transcoding::ObjectExportSettings makeSettings(
        bool isAll,
        QStringList&& typeIds = {})
    {
        return {
            .showAttributes = false,
            .typeSettings = {.isAllObjectTypes = isAll, .objectTypeIds = typeIds,},
            .taxonomyState = this->m_taxonomyState,
            .textColor = QColor(100, 0, 0),
            .frameColors = testMap};
    }

    QImage prepareImage()
    {
        QImage image(240, 180, QImage::Format_RGB32);
        image.fill(Qt::white);
        return image;
    }

    void compareImages(QImage& image, QImage& resImage, QColor color = QColor(0x0000ff))
    {
        QPainter painter(&image);
        painter.setPen(QPen(QBrush(color), 1));
        painter.drawRect(24, 18, 24, 18);
        ASSERT_EQ(image.size(), resImage.size());
        ASSERT_EQ(image.format(), resImage.format());
        ASSERT_EQ(image, resImage);
    }

    std::shared_ptr<nx::analytics::taxonomy::AbstractState> m_taxonomyState =
        TaxonomyStateWatcher().state();

    nx::vms::common::DetectedObjectSettingsMap testMap;

    nx::common::metadata::ObjectMetadata nonNormalizedMetadata, negativeBoundingBoxMetadata,
        emptyBoundingBoxMetadata, correctAnimalMetadata, coloredCarMetadata;
};

using namespace nx::core::transcoding;

TEST_F(ObjectInfoFilterTest, noMetadata)
{
    ObjectInfoFilter filter(makeSettings(true));
    filter.setMetadata({});
    auto image = prepareImage();
    auto referenceImage = prepareImage();
    filter.updateImage(image);
    ASSERT_EQ(referenceImage, image);
}

TEST_F(ObjectInfoFilterTest, IncorrectMetadata)
{
    ObjectInfoFilter filter(makeSettings(true));

    nx::common::metadata::ObjectMetadataPacket packet;
    packet.objectMetadataList.push_back(emptyBoundingBoxMetadata);
    packet.objectMetadataList.push_back(nonNormalizedMetadata);
    packet.objectMetadataList.push_back(negativeBoundingBoxMetadata);
    auto metadata = generateMetadataList(packet);
    filter.setMetadata(metadata);

    CLVideoDecoderOutputPtr frame(new CLVideoDecoderOutput(prepareImage()));
    auto resImage = filter.updateImage(frame)->toImage();
    ASSERT_EQ(frame->toImage(), resImage);
}

TEST_F(ObjectInfoFilterTest, TypeMismatch)
{
    ObjectInfoFilter filter(makeSettings(false, {"car"}));

    nx::common::metadata::ObjectMetadataPacket packet;
    packet.objectMetadataList.push_back(correctAnimalMetadata);
    auto metadata = generateMetadataList(packet);
    filter.setMetadata(metadata);

    auto image = prepareImage();
    auto referenceImage = prepareImage();
    filter.updateImage(image);
    ASSERT_EQ(referenceImage, image);
}

TEST_F(ObjectInfoFilterTest, TypeMatch)
{
    ObjectInfoFilter filter(makeSettings(false, {"animal"}));

    nx::common::metadata::ObjectMetadataPacket packet;
    packet.objectMetadataList.push_back(correctAnimalMetadata);
    auto metadata = generateMetadataList(packet);
    filter.setMetadata(metadata);

    auto image = prepareImage();
    auto referenceImage = prepareImage();
    filter.updateImage(image);
    ASSERT_NE(image, referenceImage);
    compareImages(referenceImage, image);
}

TEST_F(ObjectInfoFilterTest, AllTypes)
{
    ObjectInfoFilter filter(makeSettings(true));

    nx::common::metadata::ObjectMetadataPacket packet;
    packet.objectMetadataList.push_back(correctAnimalMetadata);
    auto metadata = generateMetadataList(packet);
    filter.setMetadata(metadata);

    auto image = prepareImage();
    auto referenceImage = prepareImage();
    filter.updateImage(image);

    ASSERT_NE(referenceImage, image);
    compareImages(referenceImage, image);
}

TEST_F(ObjectInfoFilterTest, ColorSpecified)
{
    ObjectInfoFilter filter(makeSettings(true));

    nx::common::metadata::ObjectMetadataPacket packet;
    packet.objectMetadataList.push_back(coloredCarMetadata);
    auto metadata = generateMetadataList(packet);
    filter.setMetadata(metadata);

    auto image = prepareImage();
    auto referenceImage = prepareImage();
    filter.updateImage(image);

    ASSERT_NE(referenceImage, image);
    compareImages(referenceImage, image, QColor(0xffff00));
}
