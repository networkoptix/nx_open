// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_media_persistent_settings.h"

#include <cmath>

#include <QtGui/QAbstractTextDocumentLayout>
#include <QtGui/QGuiApplication>
#include <QtGui/QPainter>
#include <QtGui/QTextDocument>

#include <client/client_meta_types.h>
#include <nx/reflect/compare.h>
#include <nx/utils/math/math.h>
#include <nx/utils/string.h>
#include <nx/vms/common/html/html.h>

namespace {

void setDocumentText(QTextDocument* document, const QString& text)
{
    if (nx::vms::common::html::mightBeHtml(text))
        document->setHtml(text);
    else
        document->setPlainText(text);
}

std::unique_ptr<QTextDocument> createDocument(int fontSize)
{
    std::unique_ptr<QTextDocument> document(new QTextDocument);

    QFont font;
    font.setPixelSize(fontSize);
    document->setDefaultFont(font);

    return document;
}

nx::core::transcoding::OverlaySettingsPtr createRuntimeSettingsFromDocument(
    const nx::vms::client::desktop::ExportTextOverlayPersistentSettingsBase& settings,
    std::unique_ptr<QTextDocument> document)
{
    QImage targetImage(document->size().width(), document->size().height(),
        QImage::Format_ARGB32_Premultiplied);
    targetImage.fill(Qt::transparent);

    QPainter imagePainter(&targetImage);
    imagePainter.setRenderHint(QPainter::Antialiasing);
    imagePainter.setBrush(settings.background);
    imagePainter.setPen(Qt::NoPen);
    imagePainter.drawRoundedRect(
        targetImage.rect(),
        settings.roundingRadius,
        settings.roundingRadius);

    QAbstractTextDocumentLayout::PaintContext context;
    context.palette = QGuiApplication::palette();
    context.palette.setColor(QPalette::Window, Qt::transparent);
    context.palette.setColor(QPalette::WindowText, settings.foreground);
    document->documentLayout()->draw(&imagePainter, context);

    QScopedPointer<nx::core::transcoding::ImageOverlaySettings> runtimeSettings(
        new nx::core::transcoding::ImageOverlaySettings());
    runtimeSettings->offset = settings.offset;
    runtimeSettings->alignment = settings.alignment;
    runtimeSettings->image = targetImage;

    return nx::core::transcoding::OverlaySettingsPtr(runtimeSettings.take());
}

} // namespace

namespace nx::vms::client::desktop {

using OverlaySettingsPtr = nx::core::transcoding::OverlaySettingsPtr;

void ExportOverlayPersistentSettings::rescale(qreal factor)
{
    offset *= factor;
}

ExportImageOverlayPersistentSettings::ExportImageOverlayPersistentSettings():
    image(lit(":/logo.png")),
    overlayWidth(image.width())
{
}

void ExportImageOverlayPersistentSettings::rescale(qreal factor)
{
    ExportOverlayPersistentSettings::rescale(factor);
    overlayWidth = qRound(overlayWidth * factor);
}

bool ExportImageOverlayPersistentSettings::operator==(
    const ExportImageOverlayPersistentSettings& other) const
{
    return nx::reflect::equals(*this, other);
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

void ExportTextOverlayPersistentSettingsBase::rescale(qreal factor)
{
    ExportOverlayPersistentSettings::rescale(factor);

    const auto oldFontSize = fontSize;
    fontSize = qMax(qRound(fontSize * factor), minimumFontSize());
    if (fontSize == oldFontSize)
        return;

    const auto effectiveFactor = qreal(fontSize) / oldFontSize;
    overlayWidth = qRound(overlayWidth * effectiveFactor);

    static constexpr int kMinimumRoundingRadius = 1;
    static constexpr int kMinimumIndent = 4;
    indent = qMax(fontSize / 4, kMinimumIndent);
    roundingRadius = qMax(fontSize / 8, kMinimumRoundingRadius);
}

bool ExportTextOverlayPersistentSettingsBase::operator==(
    const ExportTextOverlayPersistentSettingsBase& other) const
{
    return nx::reflect::equals(*this, other);
}

OverlaySettingsPtr ExportTextOverlayPersistentSettingsBase::createRuntimeSettings() const
{
    std::unique_ptr<QTextDocument> document = createDocument(fontSize);
    setDocumentText(document.get(), text);

    document->setTextWidth(overlayWidth);
    document->setDocumentMargin(indent);

    return createRuntimeSettingsFromDocument(*this, std::move(document));
}

ExportBookmarkOverlayPersistentSettings::ExportBookmarkOverlayPersistentSettings()
{
    foreground = Qt::white;
    background = QColor(0x2E, 0x69, 0x96, 0xB3);
}

void ExportTimestampOverlayPersistentSettings::rescale(qreal factor)
{
    ExportOverlayPersistentSettings::rescale(factor);
    fontSize = qMax(qRound(fontSize * factor), minimumFontSize());
}

OverlaySettingsPtr ExportTimestampOverlayPersistentSettings::createRuntimeSettings() const
{
    QScopedPointer<nx::core::transcoding::TimestampOverlaySettings> runtimeSettings(
        new nx::core::transcoding::TimestampOverlaySettings());

    runtimeSettings->offset = offset;
    runtimeSettings->alignment = alignment;
    runtimeSettings->format = format;
    runtimeSettings->timeZone = timeZone;
    runtimeSettings->fontSize = fontSize;
    runtimeSettings->foreground = foreground;
    runtimeSettings->outline = outline;

    return OverlaySettingsPtr(runtimeSettings.take());
}

nx::core::transcoding::OverlaySettingsPtr
ExportInfoOverlayPersistentSettings::createRuntimeSettings() const
{
    std::unique_ptr<QTextDocument> document = createDocument(fontSize);

    setDocumentText(document.get(), formatInfoText(cameraNameText, dateText));

    document->setDocumentMargin(indent);
    document->setTextWidth(qMin<qreal>(document->size().width(), maxOverlayWidth));

    if (document->size().height() > maxOverlayHeight)
        fitDocumentHeight(document.get(), maxOverlayHeight);

    return createRuntimeSettingsFromDocument(*this, std::move(document));
}

void ExportInfoOverlayPersistentSettings::rescale(qreal factor)
{
    ExportOverlayPersistentSettings::rescale(factor);

    const int oldFontSize = fontSize;
    fontSize = qMax(qRound(fontSize * factor), minimumFontSize());
    if (fontSize == oldFontSize)
        return;

    const auto effectiveFactor = qreal(fontSize) / oldFontSize;
    overlayWidth = qRound(overlayWidth * effectiveFactor);

    static constexpr int kMinimumRoundingRadius = 1;
    static constexpr int kMinimumIndent = 4;
    indent = qMax(fontSize / 4, kMinimumIndent);
    roundingRadius = qMax(fontSize / 8, kMinimumRoundingRadius);
}

// Form info text. Info layout size must be less than video frame size.
// Therefore it elides info text, if it is too big.
// Elides cameraName first. Then, elides export date, if the document is still too high.
void ExportInfoOverlayPersistentSettings::fitDocumentHeight(
    QTextDocument* document,
    int maxHeight) const
{
    // Rough binary search of document text maximum size.
    const int boundingLength = document->toPlainText().length();
    int lastStep = boundingLength / 2;
    int maxLength = boundingLength - lastStep;
    while (true)
    {
        setDocumentText(document, getElidedText(maxLength));

        lastStep /= 2;
        if (lastStep <= 1)
            break;

        if (document->size().height() > maxHeight)
            maxLength -= lastStep;
        else if (document->size().height() <= maxHeight)
            maxLength += lastStep;
    }

    // Exact search of document text maximum size (search step = 1 symbol).
    if (document->size().height() > maxHeight)
    {
        while (document->size().height() > maxHeight
            && document->toPlainText().length() > 0
            && maxLength > 0)
        {
            setDocumentText(document, getElidedText(--maxLength));
        }
    }
    else
    {
        while (document->size().height() <= maxHeight && qBetween(1, maxLength, boundingLength + 1))
            setDocumentText(document, getElidedText(++maxLength));
        setDocumentText(document, getElidedText(--maxLength));
    }
}

QString ExportInfoOverlayPersistentSettings::getElidedText(int maxLength) const
{
    static const QString kTail("~");

    QString elidedCameraNameText;
    QString elidedDateText;

    if (exportDate)
    {
        elidedDateText = nx::utils::elideString(dateText, maxLength, kTail);
        maxLength -= elidedDateText.length();
    }

    if (exportCameraName)
    {
        elidedCameraNameText = nx::utils::elideString(cameraNameText, maxLength, kTail);
        if (elidedCameraNameText == kTail)
            elidedCameraNameText.clear();
    }

    return formatInfoText(elidedCameraNameText, elidedDateText);
}

QString ExportInfoOverlayPersistentSettings::formatInfoText(
    const QString& cameraName,
    const QString& date) const
{
    using namespace nx::vms::common;

    QString result;
    if (!cameraName.isEmpty())
        result = html::bold(cameraName);
    if (!date.isEmpty())
    {
        if (!result.isEmpty())
            result += html::kLineBreak;
        result += date;
    }
    return result;
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
        case ExportOverlayType::info:
            return &infoOverlay;
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
        case ExportOverlayType::info:
            return &infoOverlay;
        default:
            return nullptr;
    }
}

void ExportMediaPersistentSettings::setDimension(int newDimension)
{
    if (dimension == newDimension)
        return;

    const auto factor = qreal(newDimension) / dimension;
    dimension = newDimension;

    for (int i = 0; i < int(ExportOverlayType::overlayCount); ++i)
        overlaySettings(ExportOverlayType(i))->rescale(factor);
}

bool ExportMediaPersistentSettings::canExportOverlays() const
{
    return !areFiltersForced() && hasVideo;
}

void ExportMediaPersistentSettings::updateRuntimeSettings(ExportMediaSettings& runtimeSettings) const
{
    auto& overlays = runtimeSettings.transcodingSettings.overlays;

    overlays.clear();

    if (canExportOverlays())
    {
        overlays.reserve(usedOverlays.size());

        for (const auto type : usedOverlays)
            overlays << overlaySettings(type)->createRuntimeSettings();
    }
}

bool ExportMediaPersistentSettings::areFiltersForced() const
{
    auto extension = FileSystemStrings::extension(fileFormat);
    return FileExtensionUtils::isLayout(extension);
}

bool ExportMediaPersistentSettings::setTranscoding(bool value)
{
    if (applyFilters == value)
        return false;

    applyFilters = value;
    return true;
}

ExportRapidReviewPersistentSettings::ExportRapidReviewPersistentSettings(bool enabled, int speed):
    enabled(enabled),
    speed(speed)
{
}

} // namespace nx::vms::client::desktop
