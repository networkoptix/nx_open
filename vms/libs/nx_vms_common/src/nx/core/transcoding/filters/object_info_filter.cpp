// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_info_filter.h"

#include <optional>

#include <QtGui/QImage>
#include <QtGui/QPainterPath>
#include <QtGui/QTextFrame>

#include <analytics/common/object_metadata.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/media/ffmpeg/frame_info.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/utils/object_painter_helper.h>
#include <transcoding/transcoding_utils.h>

#include "image_to_frame_painter.h"

namespace nx::core::transcoding {
namespace {

static constexpr int kAreaLineMinWidth = 1;
static constexpr int kAreaLineDivider = 1000;

static constexpr int kMaxTextLinesInFrame = 64;
static constexpr int kMinTextHeight = 8;

static constexpr int kRoundingRadius = 2;
static constexpr qreal kArrowMargin = 6;
static constexpr QSize kArrowSize(16, 8);
static constexpr qreal kArrowHalfSize = kArrowSize.width() / 2;
static constexpr QMargins kTooltipMargins(kArrowHalfSize + 6, kArrowHalfSize + 4,
    kArrowHalfSize + 6, kArrowHalfSize + 4);
static constexpr int kMaxAttributesCount = 15;

qreal getTopPosition(const QRectF& tooltipRect, const QRectF& objectRect)
{
    return qBound(
        tooltipRect.top() + kArrowMargin,
        qBound(
            objectRect.top() - kArrowHalfSize,
            objectRect.top(),
            objectRect.bottom() - kArrowHalfSize),
        tooltipRect.bottom() - kArrowMargin - kArrowSize.width());
}

qreal getLeftPosition(const QRectF& tooltipRect, const QRectF& objectRect)
{
    return qBound(
        tooltipRect.left() + kArrowMargin,
        qBound(
            objectRect.left() - kArrowHalfSize,
            objectRect.left(),
            objectRect.right() - kArrowHalfSize),
        tooltipRect.right() - kArrowMargin - kArrowSize.width());
}

void paintArrow(QPainter* painter, const QRectF& tooltipGeometry, const QRectF& frameRect)
{
    QPointF points[4];

    const auto topPos = getTopPosition(tooltipGeometry, frameRect);
    const auto leftPos = getLeftPosition(tooltipGeometry, frameRect);
    if (tooltipGeometry.left() > frameRect.right())
    {
        // Left edge.
        // Here and for Right edge arrow width and height are swapped,
        // because arrow should be rotated.
        points[0] = QPointF(tooltipGeometry.left() - kArrowSize.height(), topPos + kArrowHalfSize);
        points[1] = QPointF(tooltipGeometry.left(), topPos);
        points[2] = QPointF(tooltipGeometry.left(), topPos + kArrowSize.width());
    }
    else if (tooltipGeometry.right() < frameRect.left())
    {
        // Right edge.
        points[0] =
            QPointF(tooltipGeometry.right() + kArrowSize.height(), topPos + kArrowHalfSize);
        points[1] = QPointF(tooltipGeometry.right(), topPos + kArrowSize.width());
        points[2] = QPointF(tooltipGeometry.right(), topPos);
    }
    else if (tooltipGeometry.top() > frameRect.bottom())
    {
        // Top edge.
        points[0] = QPointF(leftPos + kArrowHalfSize, tooltipGeometry.top() - kArrowSize.height());
        points[1] = QPointF(leftPos + kArrowSize.width(), tooltipGeometry.top());
        points[2] = QPointF(leftPos, tooltipGeometry.top());
    }
    else if (tooltipGeometry.bottom() < frameRect.top())
    {
        // Bottom edge.
        points[0] =
            QPointF(leftPos + kArrowHalfSize, tooltipGeometry.bottom() + kArrowSize.height());
        points[1] = QPointF(leftPos, tooltipGeometry.bottom());
        points[2] = QPointF(leftPos + kArrowSize.width(), tooltipGeometry.bottom());
    }
    else
    {
        return;
    }

    points[3] = points[0];
    painter->drawPolygon(points, 4);
}

std::optional<nx::common::metadata::ObjectMetadataPacket> objectDataPacketFromMetadata(
    const QnConstAbstractCompressedMetadataPtr& metadata)
{
    const auto compressedMetadata =
        std::dynamic_pointer_cast<const QnCompressedMetadata>(metadata);
    if (!compressedMetadata || compressedMetadata->dataSize() > std::numeric_limits<int>::max())
        return {};

    return QnUbjson::deserialized<nx::common::metadata::ObjectMetadataPacket>(
        QByteArray::fromRawData(compressedMetadata->data(), int(compressedMetadata->dataSize())));
}

QString objectDescription(const nx::common::metadata::Attributes attributes)
{
    QStringList result;
    result.reserve(kMaxAttributesCount + 1);

    int i = 0;
    for (const auto& attribute: attributes)
    {
        result.append(QString("%1\t%2").arg(attribute.name, attribute.value));
        if (i++ >= kMaxAttributesCount)
        {
            result.append("...");
            break;
        }
    }

    return result.join("\n");
}

bool objectRectIsValid(const QRectF& rect)
{
    return rect.isValid() && rect.width() <= 1 && rect.height() <= 1 && 0 <= rect.y()
        && rect.y() <= 1 && 0 <= rect.x() && rect.x() <= 1;
}

} // namespace

QSize ObjectInfoFilter::updatedResolution(const QSize& sourceSize)
{
    // TODO: @pprivalov discuss approach and implementation
    // The idea is that if the video size is too small, we should suggest increasing the
    // video resolution. The problem is that this is not always possible, for example in layout
    // export mode. Also, to perform upscaling we would likely need a dedicated filter,
    // and we must ensure there is no conflict with DownscaleFilter.
    return sourceSize;
}

ObjectInfoFilter::ObjectInfoFilter(const ObjectExportSettings& settings):
    m_settings(settings)
{
}

ObjectInfoFilter::~ObjectInfoFilter()
{
}

void ObjectInfoFilter::setMetadata(const std::list<QnConstAbstractCompressedMetadataPtr>& metadata)
{
    m_metadata = metadata;
}

QColor ObjectInfoFilter::getFrameColor(
    const nx::common::metadata::ObjectMetadata& objectMetadata) const
{
    const auto cIter = std::find_if(
        objectMetadata.attributes.begin(),
        objectMetadata.attributes.end(),
        [](const auto& attribute)
        {
            return attribute.name == "nx.sys.color";
        });

    if (cIter != objectMetadata.attributes.end())
        return QColor(cIter->value);

    const auto colorIter = m_settings.frameColors.find(objectMetadata.typeId);
    if (colorIter != m_settings.frameColors.end())
        return colorIter->second.color;

    // Server and client synchronization for object type coloring is not supported yet,
    //  so if called via server API the frame color may be undefined. Color it yellow then.
    return {0xFFFF00};
}

void ObjectInfoFilter::drawFrame(
    const QRect& box,
    const QColor& frameColor,
    int lineWidth,
    QPainter& painter)
{
    painter.setPen(QPen(QBrush(frameColor), lineWidth));
    painter.drawRect(box);
}

std::shared_ptr<QTextDocument> ObjectInfoFilter::buildDescription(
    const common::metadata::ObjectMetadata& objectMetadata,
    int height)
{
    auto iter = m_descriptions.find(objectMetadata.trackId);
    if (iter != m_descriptions.end())
        return iter->second;

    QString description;
    const auto objectType = m_settings.taxonomyState
        ? m_settings.taxonomyState->objectTypeById(objectMetadata.typeId)
        : nullptr;
    const QString title = objectType ? objectType->name() : QString();

    description = title;
    if (!title.isEmpty() && !objectMetadata.attributes.empty())
        description += "\n";

    description += objectDescription(objectMetadata.attributes);

    // Font.
    std::shared_ptr<QTextDocument> doc = std::make_shared<QTextDocument>(description);
    const int pixelSize = std::max(kMinTextHeight, height / kMaxTextLinesInFrame);
    QFont font("Roboto");
    font.setBold(true);
    font.setWeight(QFont::Weight::Normal);
    font.setPixelSize(pixelSize);
    doc->setDefaultFont(font);
    m_descriptions[objectMetadata.trackId] = doc;

    return m_descriptions[objectMetadata.trackId];
}

void ObjectInfoFilter::drawDescription(
    const QRect& frameRect,
    const QColor& frameColor,
    QPainter& painter,
    const common::metadata::ObjectMetadata& objectMetadata,
    int width,
    int height)
{
    auto doc = buildDescription(objectMetadata, height);

    auto tooltipGeometry = vms::common::ObjectPainterHelper::calculateLabelGeometry(
        {0., 0., (qreal) width, (qreal) height},
        vms::common::Geometry::dilated(doc->size(), kTooltipMargins),
        kTooltipMargins,
        frameRect);
    auto backgroundColor = vms::common::ObjectPainterHelper::calculateTooltipColor(frameColor);

    tooltipGeometry = vms::common::Geometry::eroded(tooltipGeometry, kTooltipMargins);

    // Actual drawing.
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(backgroundColor);
    painter.drawRoundedRect(tooltipGeometry, kRoundingRadius, kRoundingRadius);

    paintArrow(&painter, tooltipGeometry, frameRect);

    painter.setPen(m_settings.textColor);
    painter.translate(tooltipGeometry.topLeft());
    doc->drawContents(&painter);

    painter.restore();
}

CLVideoDecoderOutputPtr ObjectInfoFilter::updateImage(const CLVideoDecoderOutputPtr& frame)
{
    if (m_metadata.empty() || m_settings.typeSettings.empty())
        return frame;

    std::set<Uuid> oldFramesObjectsSet;
    std::transform(
        m_descriptions.begin(),
        m_descriptions.end(),
        std::inserter(oldFramesObjectsSet, oldFramesObjectsSet.end()),
        [](auto pair)
        {
            return pair.first;
        });

    const int width = frame->width;
    const int height = frame->height;
    const int lineWidth = std::max(std::min(width, height) / kAreaLineDivider, kAreaLineMinWidth);

    auto resImage = frame->toImage();
    QPainter painter(&resImage);
    for (const auto& metadata: m_metadata)
    {
        const auto objectDataPacket = objectDataPacketFromMetadata(metadata);
        if (!objectDataPacket)
            continue;

        for (const auto& objectMetadata: objectDataPacket->objectMetadataList)
        {
            oldFramesObjectsSet.erase(objectMetadata.trackId);

            if (!objectRectIsValid(objectMetadata.boundingBox)
                || !m_settings.typeSettings.contains(objectMetadata.typeId))
            {
                continue;
            }

            QRect box = {
                qRound(objectMetadata.boundingBox.x() * width),
                qRound(objectMetadata.boundingBox.y() * height),
                qRound(objectMetadata.boundingBox.width() * width),
                qRound(objectMetadata.boundingBox.height() * height)};
            box = box.intersected({0, 0, width, height});
            if (box.isEmpty())
                continue;

            const QColor frameColor = getFrameColor(objectMetadata);
            drawFrame(box, frameColor, lineWidth, painter);
            if (m_settings.showAttributes)
                drawDescription(box, frameColor, painter, objectMetadata, width, height);
        }
    }

    CLVideoDecoderOutputPtr result(new CLVideoDecoderOutput(resImage));
    result->assignMiscData(frame.get());
    for (const auto id: oldFramesObjectsSet)
        m_descriptions.erase(id);

    return result;
}

} // namespace nx::core::transcoding
