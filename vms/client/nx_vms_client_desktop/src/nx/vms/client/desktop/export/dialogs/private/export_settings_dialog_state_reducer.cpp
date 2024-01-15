// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>

#include <nx/core/layout/layout_file_info.h>
#include <nx/core/transcoding/filters/timestamp_filter.h>
#include <nx/reflect/from_string.h>
#include <nx/utils/file_system.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/common/html/html.h>

#include "export_settings_dialog_state.h"

using nx::core::transcoding::TimestampFormat;

namespace {

static const qint64 kMsInSec = 1000;

// Minimal rapid review speed. Limited by keyframes count.
static const int kMinimalSpeed = 10;

// Minimal result length.
static const qint64 kMinimalLengthMs = 1000;

static const auto kMinimalSourcePeriodLengthMs = kMinimalLengthMs * kMinimalSpeed;

// Default value for timelapse video: 5 minutes.
static const qint64 kDefaultLengthMs = 5 * 60 * 1000;

/* Magic knowledge. We know that the result will be generated with 30 fps. --rvasilenko */
static const int kResultFps = 30;

static const auto kImageCacheDirName = "export_image_overlays";
static const auto kCachedMediaOverlayImageName = "export_media_image_overlay.png";
static const auto kCachedBookmarkOverlayImageName = "export_bookmark_image_overlay.png";

struct Position
{
    int offset = 0;
    Qt::AlignmentFlag alignment = Qt::AlignmentFlag();

    Position() = default;
    Position(int offset, Qt::AlignmentFlag alignment):
        offset(offset), alignment(alignment)
    {
    }

    bool operator<(const Position& other) const
    {
        return qAbs(offset) < qAbs(other.offset);
    }
};

template<class Settings>
void copyOverlaySettingsWithoutPosition(Settings& dest, Settings src)
{
    // Position is stored in ExportOverlayPersistentSettings base.
    src.nx::vms::client::desktop::ExportOverlayPersistentSettings::operator=(dest); //< Preserve position.
    dest.operator=(src);
}

QSize textMargins(const QFontMetrics& fontMetrics)
{
    return QSize(fontMetrics.averageCharWidth() / 2, 1);
}

QString calculateLongestStringViewDateTime(TimestampFormat format)
{
    qsizetype maxLength{0};
    static const QTime kLongestTime(20, 20, 28, 888);
    for (int month = 1; month <= 12; ++month)
    {
        QDateTime dateTime(QDate(2000, month, 20), kLongestTime);
        maxLength = std::max(maxLength,
            nx::core::transcoding::TimestampFilter::timestampTextUtc(
                dateTime.toMSecsSinceEpoch(),
                QTimeZone::LocalTime,
                format).length());
    }
    return QString(maxLength, QChar('8'));
}

int calculateMaximumFontSize(TimestampFormat format, const QFont& font, int maxWidth)
{
    auto longestStringViewDateTime = calculateLongestStringViewDateTime(format);
    auto currentFont = font;
    int pixelFontSize{0};
    while (++pixelFontSize)
    {
        currentFont.setPixelSize(pixelFontSize);
        QFontMetrics fontMetrics(currentFont);
        auto textSize =
            fontMetrics.size(0, longestStringViewDateTime) + textMargins(fontMetrics) * 2;
        if (textSize.width() > maxWidth)
            break;
    }
    return --pixelFontSize;
}

} // namespace

namespace nx::vms::client::desktop {

using State = ExportSettingsDialogState;
using Reducer = ExportSettingsDialogStateReducer;

void ExportSettingsDialogState::applyTranscodingSettings()
{
    // Fixing settings for preview transcoding
    // This settings are affecting both export and preview.
    nx::core::transcoding::Settings& settings = exportMediaSettings.transcodingSettings;
    if (exportMediaPersistentSettings.applyFilters)
    {
        settings.rotation = availableTranscodingSettings.rotation;
        settings.aspectRatio = availableTranscodingSettings.aspectRatio;
        settings.enhancement = availableTranscodingSettings.enhancement;
        settings.dewarping = availableTranscodingSettings.dewarping;
        settings.zoomWindow = availableTranscodingSettings.zoomWindow;
    }
    else
    {
        settings.rotation = 0;
        settings.aspectRatio = {};
        settings.enhancement = {};
        settings.dewarping = {};
        settings.zoomWindow = {};
    }
}

void ExportSettingsDialogState::updateBookmarkText()
{
    const auto description = exportMediaPersistentSettings.bookmarkOverlay.includeDescription
        ? common::html::toHtml(bookmarkDescription)
        : QString();

    static const QString kBookmarkTemplate("<p><font size=5>%1</font></p>%2");
    const auto text = kBookmarkTemplate.arg(bookmarkName.toHtmlEscaped()).arg(description);
    exportMediaPersistentSettings.bookmarkOverlay.text = text;
}

QString ExportSettingsDialogState::cachedImageFileName() const
{
    const auto baseName = bookmarkName.isEmpty()
        ? kCachedMediaOverlayImageName
        : kCachedBookmarkOverlayImageName;

    return cacheDir.absoluteFilePath(baseName);
}

void ExportSettingsDialogState::setCacheDirLocation(const QString& location)
{
    QDir newCacheDir(location + QDir::separator() + kImageCacheDirName);

    cacheDir = nx::utils::file_system::ensureDir(newCacheDir) ? newCacheDir : QDir();
}

State ExportSettingsDialogStateReducer::loadSettings(State state,
    const LocalSettings* localSettings,
    const QString& cacheDirLocation)
{
    // TODO: #sivanov Persistent and runtime settings should be splitted better.
    const auto timeZone = state.exportMediaPersistentSettings.timestampOverlay.timeZone;
    state.exportMediaPersistentSettings = state.bookmarkName.isEmpty()
        ? localSettings->exportMediaSettings()
        : localSettings->exportBookmarkSettings();

    // Restore runtime option, overridden by persisten settings.
    state.exportMediaPersistentSettings.timestampOverlay.timeZone = timeZone;

    if (state.bookmarkName.isEmpty())
        state.exportMediaPersistentSettings.usedOverlays.removeOne(ExportOverlayType::bookmark);

    state.exportLayoutPersistentSettings = localSettings->exportLayoutSettings();

    auto lastExportDir = localSettings->lastExportDir();
    if (lastExportDir.isEmpty() && !localSettings->mediaFolders().isEmpty())
        lastExportDir = localSettings->mediaFolders().first();
    if (lastExportDir.isEmpty())
        lastExportDir = QDir::homePath();

    const auto mode = state.bookmarkName.isEmpty()
        ? localSettings->lastExportMode()
        : ExportMode::media;

    state = setMode(std::move(state), mode).second;

    state.exportMediaSettings.fileName.path = lastExportDir;
    state.exportMediaSettings.fileName.extension = FileSystemStrings::extension(
        state.exportMediaPersistentSettings.fileFormat, FileExtension::mkv);

    state.exportLayoutSettings.fileName.path = lastExportDir;
    state.exportLayoutSettings.fileName.extension = FileSystemStrings::extension(
        state.exportLayoutPersistentSettings.fileFormat, FileExtension::nov);

    state.setCacheDirLocation(cacheDirLocation);

    auto& imageOverlay = state.exportMediaPersistentSettings.imageOverlay;
    if (!imageOverlay.name.trimmed().isEmpty())
    {
        const QImage cachedImage(state.cachedImageFileName());
        if (cachedImage.isNull())
            imageOverlay.name = QString();
        else
            imageOverlay.image = cachedImage;
    }

    state.applyTranscodingSettings();
    state.updateBookmarkText();

    const auto speed = state.exportMediaPersistentSettings.rapidReview.speed;
    state = setSpeed(std::move(state), speed);

    if (state.bookmarkName.isEmpty()
        || mode == ExportMode::layout
        || nx::core::layout::isLayoutExtension(
            state.exportMediaSettings.fileName.completeFileName()))
        return clearSelection(std::move(state));

    return selectOverlay(std::move(state), ExportOverlayType::bookmark);
}

State ExportSettingsDialogStateReducer::setTimestampTimeZone(
    State state,
    const QTimeZone &timeZone)
{
    state.exportMediaPersistentSettings.timestampOverlay.timeZone = timeZone;
    return state;
}

std::pair<bool, State> ExportSettingsDialogStateReducer::setApplyFilters(State state, bool value)
{
    bool transcode = false;
    if (state.exportMediaPersistentSettings.areFiltersForced())
        transcode = true;
    else
        transcode = value;

    if (state.exportMediaPersistentSettings.setTranscoding(transcode))
    {
        state.applyTranscodingSettings();
        return {true, std::move(state)};
    }

    return {false, std::move(state)};
}

State ExportSettingsDialogStateReducer::setTimestampOverlaySettings(
    State state,
    const ExportTimestampOverlayPersistentSettings& settings)
{
    copyOverlaySettingsWithoutPosition(state.exportMediaPersistentSettings.timestampOverlay, settings);
    return state;
}

State ExportSettingsDialogStateReducer::setImageOverlaySettings(
    State state,
    const ExportImageOverlayPersistentSettings& settings)
{
    copyOverlaySettingsWithoutPosition(state.exportMediaPersistentSettings.imageOverlay, settings);
    return state;
}

State ExportSettingsDialogStateReducer::setTextOverlaySettings(
    State state,
    const ExportTextOverlayPersistentSettings& settings)
{
    copyOverlaySettingsWithoutPosition(state.exportMediaPersistentSettings.textOverlay, settings);
    return state;
}

State ExportSettingsDialogStateReducer::setBookmarkOverlaySettings(
    State state,
    const ExportBookmarkOverlayPersistentSettings& settings)
{
    copyOverlaySettingsWithoutPosition(state.exportMediaPersistentSettings.bookmarkOverlay, settings);
    state.updateBookmarkText();
    return state;
}

State ExportSettingsDialogStateReducer::setInfoOverlaySettings(
    State state,
    const ExportInfoOverlayPersistentSettings& settings)
{
    copyOverlaySettingsWithoutPosition(state.exportMediaPersistentSettings.infoOverlay, settings);
    return state;
}

State ExportSettingsDialogStateReducer::setLayoutReadOnly(State state, bool value)
{
    state.exportLayoutPersistentSettings.readOnly = value;
    return state;
}

State ExportSettingsDialogStateReducer::setLayoutEncryption(State state, bool on, const QString& password)
{
    state.exportLayoutSettings.encryption.on = on;
    state.exportLayoutSettings.encryption.password = password;
    return state;
}

State ExportSettingsDialogStateReducer::setBookmarks(State state, const QnCameraBookmarkList& bookmarks)
{
    state.exportLayoutSettings.bookmarks = bookmarks;
    return state;
}

State ExportSettingsDialogStateReducer::setMediaResourceSettings(State state, bool hasVideo, const nx::core::transcoding::Settings& settings)
{
    state.availableTranscodingSettings = settings;
    state.exportMediaPersistentSettings.hasVideo = hasVideo;

    state.applyTranscodingSettings();

    return state;
}

std::pair<bool, State> ExportSettingsDialogStateReducer::setFrameSize(State state, const QSize& size)
{
    if (state.fullFrameSize == size)
        return {false, std::move(state)};

    if (size.isValid() && state.fullFrameSize.isValid())
    {
        const auto newDimension = std::min(size.width(), size.height());
        state.exportMediaPersistentSettings.setDimension(newDimension);
    }

    state.fullFrameSize = size;

    const QPair<qreal, qreal> coefficients(
        qreal(state.previewSize.width()) / state.fullFrameSize.width(),
        qreal(state.previewSize.height()) / state.fullFrameSize.height());

    state.overlayScale = std::min({coefficients.first, coefficients.second, 1.0});
    //If fullFrameSize was updated then need update maximum font size for timestamp
    return {true, updateMaximumFontSizeTimestamp(std::move(state))};
}

State ExportSettingsDialogStateReducer::setTimePeriod(State state, const QnTimePeriod& period)
{
    state.exportMediaSettings.period = period;
    state.exportLayoutSettings.period = period;
    const auto sourcePeriodLengthMs = state.exportMediaSettings.period.duration().count();

    if (sourcePeriodLengthMs < kMinimalSourcePeriodLengthMs)
    {
        state.exportMediaPersistentSettings.rapidReview.speed = 1;
        state.exportMediaSettings.timelapseFrameStepMs = 0;
        return state;
    }

    return setSpeed(std::move(state), sourcePeriodLengthMs / kDefaultLengthMs);
}

State ExportSettingsDialogStateReducer::setLayoutFilename(State state, const Filename& filename)
{
    state.exportLayoutSettings.fileName = filename;
    state.exportLayoutPersistentSettings.fileFormat = FileSystemStrings::suffix(filename.extension);
    return state;
}

State ExportSettingsDialogStateReducer::setSpeed(State state, int speed)
{
    const auto sourcePeriodLengthMs = state.exportMediaSettings.period.duration().count();
    const auto maxSpeed = sourcePeriodLengthMs / kMinimalLengthMs;

    if (speed < kMinimalSpeed)
        speed = kMinimalSpeed;
    else if (speed > maxSpeed)
        speed = maxSpeed;

    state.exportMediaPersistentSettings.rapidReview.speed = speed;
    state.exportMediaSettings.timelapseFrameStepMs = state.exportMediaPersistentSettings.rapidReview.enabled ? kMsInSec * speed / kResultFps : 0;

    return state;
}

std::pair<bool, State> ExportSettingsDialogStateReducer::setMode(State state, ExportMode mode)
{
    const bool mediaAllowed = state.mediaAvailable && state.mediaDisableReason.isEmpty();
    if (mode == ExportMode::media && !mediaAllowed)
        mode = ExportMode::layout;
    const bool layoutAllowed = state.layoutAvailable && state.layoutDisableReason.isEmpty();
    if (mode == ExportMode::layout && !layoutAllowed)
        mode = ExportMode::media;

    if (state.mode == mode)
        return {false, std::move(state)};

    state.mode = mode;
    return {true, std::move(state)};
}

std::pair<bool, State> ExportSettingsDialogStateReducer::setOverlayOffset(State state, ExportOverlayType type, int x, int y)
{
    auto settings = state.exportMediaPersistentSettings.overlaySettings(type);
    NX_ASSERT(settings);
    if (!settings)
        return {false, std::move(state)};

    settings->offset = QPoint(x, y);
    settings->alignment = Qt::AlignLeft | Qt::AlignTop;
    return {true, std::move(state)};
}

// Computes offset-alignment pair from absolute pixel position
std::pair<bool, State> ExportSettingsDialogStateReducer::overlayPositionChanged(State state, ExportOverlayType type, QRect geometry, QSize space)
{
    if (!state.fullFrameSize.isValid())
        return {false, std::move(state)};

    auto settings = state.exportMediaPersistentSettings.overlaySettings(type);
    NX_ASSERT(settings);
    if (!settings)
        return {false, std::move(state)};

    const auto beg = geometry.topLeft();
    const auto end = geometry.bottomRight();
    const auto mid = geometry.center();

    const auto horizontal = std::min({
        Position(beg.x(), Qt::AlignLeft),
        Position(mid.x() - space.width() / 2, Qt::AlignHCenter),
        Position(space.width() - end.x() - 1, Qt::AlignRight)});

    const auto vertical = std::min({
        Position(beg.y(), Qt::AlignTop),
        Position(mid.y() - space.height() / 2, Qt::AlignVCenter),
        Position(space.height() - end.y() - 1, Qt::AlignBottom)});

    settings->offset = QPoint(horizontal.offset, vertical.offset) / state.overlayScale;
    settings->alignment = horizontal.alignment | vertical.alignment;
    return {true, std::move(state)};
}

State ExportSettingsDialogStateReducer::clearSelection(State state)
{
    state.selectedOverlayType = ExportOverlayType::none;
    state.rapidReviewSelected = false;
    return state;
}

State ExportSettingsDialogStateReducer::selectOverlay(State state, ExportOverlayType type)
{
    state.selectedOverlayType = type;

    if (type != ExportOverlayType::none)
    {
        // Put type at the end
        state.exportMediaPersistentSettings.usedOverlays.removeOne(type);
        state.exportMediaPersistentSettings.usedOverlays.push_back(type);
        state.rapidReviewSelected = false;
    }

    return state;
}

State ExportSettingsDialogStateReducer::hideOverlay(State state, ExportOverlayType type)
{
    state.exportMediaPersistentSettings.usedOverlays.removeOne(type);
    return state;
}

State ExportSettingsDialogStateReducer::selectRapidReview(State state)
{
    state.selectedOverlayType = ExportOverlayType::none;
    state.rapidReviewSelected = true;
    state.exportMediaPersistentSettings.rapidReview.enabled = true;
    return setSpeed(std::move(state), state.exportMediaPersistentSettings.rapidReview.speed);
}

State ExportSettingsDialogStateReducer::hideRapidReview(State state)
{
    state.rapidReviewSelected = false;
    state.exportMediaPersistentSettings.rapidReview.enabled = false;
    return setSpeed(std::move(state), state.exportMediaPersistentSettings.rapidReview.speed);
}

State ExportSettingsDialogStateReducer::setMediaFilename(State state, const Filename& filename)
{
    state.exportMediaSettings.fileName = filename;
    state.exportMediaPersistentSettings.fileFormat = FileSystemStrings::suffix(filename.extension);

    if (nx::core::layout::isLayoutExtension(filename.completeFileName()))
        state = clearSelection(std::move(state));

    bool needTranscoding = state.exportMediaSettings.transcodingSettings.watermark.visible()
        || state.exportMediaPersistentSettings.applyFilters
        || state.exportMediaPersistentSettings.areFiltersForced();

    if (state.exportMediaPersistentSettings.setTranscoding(needTranscoding))
        state.applyTranscodingSettings();

    return state;
}

State ExportSettingsDialogStateReducer::enableTab(State state, ExportMode mode)
{
    if (mode == ExportMode::media)
    {
        state.mediaAvailable = true;
        state.mediaDisableReason = QString();
    }
    else
    {
        state.layoutAvailable = true;
        state.layoutDisableReason = QString();
    }
    return state;
}

State ExportSettingsDialogStateReducer::disableTab(
    State state, ExportMode mode, const QString& reason)
{
    if (mode == ExportMode::media)
    {
        state.mediaAvailable = true;
        state.mediaDisableReason = reason;
    }
    else
    {
        state.layoutAvailable = true;
        state.layoutDisableReason = reason;
    }
    return state;
}

State ExportSettingsDialogStateReducer::setTimestampFont(State state, const QFont& font)
{
    state.exportMediaPersistentSettings.timestampOverlay.font = font;
    return updateMaximumFontSizeTimestamp(std::move(state));
}

State ExportSettingsDialogStateReducer::updateMaximumFontSizeTimestamp(State state)
{
    auto& timestampOverlay = state.exportMediaPersistentSettings.timestampOverlay;
    timestampOverlay.maximumFontSize = calculateMaximumFontSize(
        timestampOverlay.format, timestampOverlay.font, state.fullFrameSize.width());
    timestampOverlay.fontSize =
        std::min(timestampOverlay.fontSize, timestampOverlay.maximumFontSize);
    return state;
}

State ExportSettingsDialogStateReducer::setFormatTimestampOverlay(
    State state,
    TimestampFormat format)
{
    state.exportMediaPersistentSettings.timestampOverlay.format = format;
    return updateMaximumFontSizeTimestamp(std::move(state));
}

} // nx::vms::client::desktop
