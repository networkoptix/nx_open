#include "export_media_persistent_settings.h"

#include <cmath>

#include <QtGui/QPainter>
#include <QtGui/QTextDocument>
#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QGuiApplication>

#include <client/client_meta_types.h>
#include <nx/fusion/model_functions.h>
#include <utils/common/html.h>

namespace nx {
namespace client {
namespace desktop {

using OverlaySettingsPtr = nx::core::transcoding::OverlaySettingsPtr;

ExportImageOverlayPersistentSettings::ExportImageOverlayPersistentSettings():
    image(lit(":/logo.png")),
    overlayWidth(image.width())
{
}

OverlaySettingsPtr ExportImageOverlayPersistentSettings::createRuntimeSettings() const
{
    const qreal aspectRatio = image.isNull() ? 1.0 : qreal(image.width()) / image.height();
    const qreal overlayHeight = overlayWidth / aspectRatio;

    QScopedPointer<nx::core::transcoding::ImageOverlaySettings> runtimeSettings(
        new nx::core::transcoding::ImageOverlaySettings());
    runtimeSettings->offset = offset;
    runtimeSettings->alignment = alignment;

    if (image.width() == overlayWidth && qFuzzyIsNull(opacity - 1.0)
        && image.format() == QImage::Format_ARGB32_Premultiplied)
    {
        runtimeSettings->image = image;
    }
    else
    {
        runtimeSettings->image = QImage(overlayWidth, std::ceil(overlayHeight),
            QImage::Format_ARGB32_Premultiplied);
        runtimeSettings->image.fill(Qt::transparent);

        QPainter imagePainter(&runtimeSettings->image);
        imagePainter.setRenderHint(QPainter::SmoothPixmapTransform);
        imagePainter.setOpacity(opacity);
        imagePainter.drawImage(QRectF(QPointF(), QSizeF(overlayWidth, overlayHeight)), image);
        imagePainter.end();
    }

    return OverlaySettingsPtr(runtimeSettings.take());
}

OverlaySettingsPtr ExportTextOverlayPersistentSettingsBase::createRuntimeSettings() const
{
    QScopedPointer<QTextDocument> document(new QTextDocument);
    if (mightBeHtml(text))
        document->setHtml(text);
    else
        document->setPlainText(text);

    document->setTextWidth(overlayWidth);
    document->setDocumentMargin(indent);

    QFont font;
    font.setPixelSize(fontSize);
    document->setDefaultFont(font);

    QImage targetImage(overlayWidth, document->size().height(),
        QImage::Format_ARGB32_Premultiplied);
    targetImage.fill(Qt::transparent);

    QPainter imagePainter(&targetImage);
    imagePainter.setRenderHint(QPainter::Antialiasing);
    imagePainter.setBrush(background);
    imagePainter.setPen(Qt::NoPen);
    imagePainter.drawRoundedRect(targetImage.rect(), roundingRadius, roundingRadius);

    QAbstractTextDocumentLayout::PaintContext context;
    context.palette = QGuiApplication::palette();
    context.palette.setColor(QPalette::Window, Qt::transparent);
    context.palette.setColor(QPalette::WindowText, foreground);
    document->documentLayout()->draw(&imagePainter, context);

    QScopedPointer<nx::core::transcoding::ImageOverlaySettings> runtimeSettings(
        new nx::core::transcoding::ImageOverlaySettings());
    runtimeSettings->offset = offset;
    runtimeSettings->alignment = alignment;
    runtimeSettings->image = targetImage;

    return OverlaySettingsPtr(runtimeSettings.take());
}

ExportBookmarkOverlayPersistentSettings::ExportBookmarkOverlayPersistentSettings()
{
    foreground = Qt::white;
    background = QColor(0x2E, 0x69, 0x96, 0xB3);
}

OverlaySettingsPtr ExportTimestampOverlayPersistentSettings::createRuntimeSettings() const
{
    QScopedPointer<nx::core::transcoding::TimestampOverlaySettings> runtimeSettings(
        new nx::core::transcoding::TimestampOverlaySettings());

    runtimeSettings->offset = offset;
    runtimeSettings->alignment = alignment;
    runtimeSettings->format = format;
    runtimeSettings->serverTimeDisplayOffsetMs = serverTimeDisplayOffsetMs;
    runtimeSettings->fontSize = fontSize;
    runtimeSettings->foreground = foreground;
    runtimeSettings->outline = outline;

    return OverlaySettingsPtr(runtimeSettings.take());
}

ExportOverlayPersistentSettings* ExportMediaPersistentSettings::overlaySettings(
    ExportOverlayType type)
{
    switch (type)
    {
        case ExportOverlayType::timestamp:
            return &timestampOverlay;
        case ExportOverlayType::image:
            return &imageOverlay;
        case ExportOverlayType::text:
            return &textOverlay;
        case ExportOverlayType::bookmark:
            return &bookmarkOverlay;
        default:
            return nullptr;
    }
}

const ExportOverlayPersistentSettings* ExportMediaPersistentSettings::overlaySettings(
    ExportOverlayType type) const
{
    switch (type)
    {
        case ExportOverlayType::timestamp:
            return &timestampOverlay;
        case ExportOverlayType::image:
            return &imageOverlay;
        case ExportOverlayType::text:
            return &textOverlay;
        case ExportOverlayType::bookmark:
            return &bookmarkOverlay;
        default:
            return nullptr;
    }
}

void ExportMediaPersistentSettings::updateRuntimeSettings(ExportMediaSettings& runtimeSettings) const
{
    auto& overlays = runtimeSettings.transcodingSettings.overlays;

    overlays.clear();
    overlays.reserve(usedOverlays.size());

    for (const auto type: usedOverlays)
        overlays << overlaySettings(type)->createRuntimeSettings();
}

ExportRapidReviewPersistentSettings::ExportRapidReviewPersistentSettings(bool enabled, int speed):
    enabled(enabled),
    speed(speed)
{
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(EXPORT_MEDIA_PERSISTENT_TYPES, (json), _Fields)

} // namespace desktop
} // namespace client
} // namespace nx

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::client::desktop, ExportOverlayType,
    (nx::client::desktop::ExportOverlayType::timestamp, "timestamp")
    (nx::client::desktop::ExportOverlayType::image, "image")
    (nx::client::desktop::ExportOverlayType::text, "text")
    (nx::client::desktop::ExportOverlayType::bookmark, "bookmark"))
