// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "export_settings_dialog_state.h"

#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>

#include <client/client_settings.h>
#include <nx/core/layout/layout_file_info.h>
#include <nx/core/transcoding/filters/timestamp_filter.h>
#include <nx/reflect/from_string.h>
#include <nx/utils/file_system.h>
#include <nx/vms/common/html/html.h>

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

static const auto kImageCacheDirName = lit("export_image_overlays");
static const auto kCachedMediaOverlayImageName = lit("export_media_image_overlay.png");
static const auto kCachedBookmarkOverlayImageName = lit("export_bookmark_image_overlay.png");

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

QString calculateLongestStringViewDateTime(const Qt::DateFormat& format)
{
    int maxLength{0};
    static const QTime kLongestTime(20, 20, 28, 888);
    for (int month = 1; month <= 12; ++month)
    {
        QDateTime dateTime(QDate(2000, month, 20), kLongestTime);
        maxLength = std::max(maxLength,
            nx::core::transcoding::TimestampFilter::timestampTextUtc(
                dateTime.toMSecsSinceEpoch(), 0, format)
                .length());
    }
    return QString(maxLength, QChar('8'));
}

int calculateMaximumFontSize(const Qt::DateFormat& format, const QFont& font, int maxWidth)
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

    static const auto kBookmarkTemplate = lit("<p><font size=5>%1</font></p>%2");
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

State ExportSettingsDialogStateReducer::loadSettings(
    State state, const QnClientSettings* settings, const QString& cacheDirLocation)
{
    qint64 timestampOffsetMs =
        state.exportMediaPersistentSettings.timestampOverlay.serverTimeDisplayOffsetMs;
    state.exportMediaPersistentSettings = state.bookmarkName.isEmpty()
        ? settings->exportMediaSettings()
        : settings->exportBookmarkSettings();
    state.exportMediaPersistentSettings.timestampOverlay.serverTimeDisplayOffsetMs =
        timestampOffsetMs;

    if (state.bookmarkName.isEmpty())
        state.exportMediaPersistentSettings.usedOverlays.removeOne(ExportOverlayType::bookmark);

    state.exportLayoutPersistentSettings = settings->exportLayoutSettings();

    auto lastExportDir = settings->lastExportDir();
    if (lastExportDir.isEmpty() && !settings->mediaFolders().isEmpty())
        lastExportDir = settings->mediaFolders().first();
    if (lastExportDir.isEmpty())
        lastExportDir = QDir::homePath();

    const auto mode = state.bookmarkName.isEmpty()
        ? nx::reflect::fromString<ExportSettingsDialog::Mode>(
            settings->lastExportMode().toStdString(), ExportSettingsDialog::Mode::Media)
        : ExportSettingsDialog::Mode::Media;

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

    state = setSpeed(std::move(state), state.exportMediaPersistentSettings.rapidReview.speed);

    if (state.bookmarkName.isEmpty()
        || mode == ExportSettingsDialog::Mode::Layout
        || nx::core::layout::isLayoutExtension(
            state.exportMediaSettings.fileName.completeFileName()))
        return clearSelection(std::move(state));

    return selectOverlay(std::move(state), ExportOverlayType::bookmark);
}

State ExportSettingsDialogStateReducer::setServerTimeZoneOffsetMs(State state, qint64 offsetMs)
{
    state.exportMediaSettings.serverTimeZoneMs = offsetMs;
    return state;
}

State ExportSettingsDialogStateReducer::setTimestampOffsetMs(State state, qint64 offsetMs)
{
    state.exportMediaPersistentSettings.timestampOverlay.serverTimeDisplayOffsetMs = offsetMs;
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

std::pair<bool, State> ExportSettingsDialogStateReducer::setMode(State state, ExportSettingsDialog::Mode mode)
{
    const bool mediaAllowed = state.mediaAvailable == true && state.mediaDisableReason.isEmpty();
    if (mode == ExportSettingsDialog::Mode::Media && !mediaAllowed)
        mode = ExportSettingsDialog::Mode::Layout;
    const bool layoutAllowed = state.layoutAvailable == true && state.layoutDisableReason.isEmpty();
    if (mode == ExportSettingsDialog::Mode::Layout && !layoutAllowed)
        mode = ExportSettingsDialog::Mode::Media;

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

State ExportSettingsDialogStateReducer::enableTab(State state, ExportSettingsDialog::Mode mode)
{
    if (mode == ExportSettingsDialog::Mode::Media)
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

State ExportSettingsDialogStateReducer::disableTab(State state, ExportSettingsDialog::Mode mode, const QString& reason)
{
    if (mode == ExportSettingsDialog::Mode::Media)
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

namespace {

typedef std::function<State(State, const QVariant&)> ParseStateFunc;

State parseMap(State state, const QVariant& v, const QMap<QString, ParseStateFunc>& rules)
{
    const QVariantMap m = v.toMap();

    if (v.isNull())
        return state;

    QMapIterator<QString, ParseStateFunc> i(rules);

    // Call ParseStateFunc for each property found in the rules.
    while (i.hasNext())
    {
        i.next();
        const auto valueIt = m.find(i.key());
        if (valueIt == m.end())
            continue;
        state = i.value()(std::move(state), *valueIt);
    }

    return state;
}

State parseList(State state, const QVariant& v, const QString& typeProperty, const QMap<QString, ParseStateFunc>& rules)
{
    const QVariantList l = v.toList();

    // Call ParseStateFunc for the object if its typeProperty field is in the rules.
    foreach(const QVariant &o, l)
    {
        const auto item = o.toMap();
        const auto valueIt = rules.find(item.value(typeProperty).toString());
        if (valueIt == rules.end())
            continue;
        state = valueIt.value()(std::move(state), item);
    }

    return state;
}

State selectOverlayWithCoords(State state, const QVariant& v, ExportOverlayType overlayType)
{
    int left = 0;
    int top = 0;

    state = parseMap(std::move(state), v, {
        { "left", [&](State state, const QVariant& v){ left = v.toInt(); return state; } },
        { "top", [&](State state, const QVariant& v){ top = v.toInt(); return state; } }
    });

    state = Reducer::setOverlayOffset(std::move(state), overlayType, left, top).second;
    return Reducer::selectOverlay(std::move(state), overlayType);
}

State parseImageOverlay(State state, const QVariant& v)
{
    ExportImageOverlayPersistentSettings imageSettings = state.exportMediaPersistentSettings.imageOverlay;

    state.setCacheDirLocation(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));

    state = parseMap(std::move(state), v, {
        { "opacity", [&](State state, const QVariant& v){ imageSettings.opacity = v.toFloat(); return state; } },
        { "size", [&](State state, const QVariant& v){ imageSettings.overlayWidth = v.toInt(); return state; } },
        { "path", [&](State state, const QVariant& v){
            QFileInfo fileInfo(v.toString());
            imageSettings.image.load(fileInfo.absoluteFilePath());
            imageSettings.name = fileInfo.fileName();
            return state;
        } },
        { "cacheDirLocation", [&](State state, const QVariant& v){
            state.setCacheDirLocation(v.toString());
            return state;
        } }
    });

    state = Reducer::setImageOverlaySettings(std::move(state), imageSettings);

    return selectOverlayWithCoords(std::move(state), v, ExportOverlayType::image);
}

State parseTextOverlay(State state, const QVariant& v)
{
    ExportTextOverlayPersistentSettings textSettings = state.exportMediaPersistentSettings.textOverlay;

    state = parseMap(std::move(state), v, {
        { "fontSize", [&](State state, const QVariant& v){ textSettings.fontSize = v.toInt(); return state; } },
        { "areaWidth", [&](State state, const QVariant& v){ textSettings.overlayWidth = v.toInt(); return state; } },
        { "text", [&](State state, const QVariant& v){ textSettings.text = v.toString(); return state; } }
    });

    state = Reducer::setTextOverlaySettings(std::move(state), textSettings);

    return selectOverlayWithCoords(std::move(state), v, ExportOverlayType::text);
}

State parseBookmarkOverlay(State state, const QVariant& v)
{
    ExportBookmarkOverlayPersistentSettings bookmarkSettings = state.exportMediaPersistentSettings.bookmarkOverlay;

    state = parseMap(std::move(state), v, {
        { "includeDescription", [&](State state, const QVariant& v){ bookmarkSettings.includeDescription = v.toBool(); return state; } },
        { "areaWidth", [&](State state, const QVariant& v){ bookmarkSettings.overlayWidth = v.toInt(); return state; } },
        { "fontSize", [&](State state, const QVariant& v){ bookmarkSettings.fontSize = v.toInt(); return state; } }
    });

    state = Reducer::setBookmarkOverlaySettings(std::move(state), bookmarkSettings);

    return selectOverlayWithCoords(std::move(state), v, ExportOverlayType::bookmark);
}

State parseTimestampOverlay(State state, const QVariant& v)
{
    ExportTimestampOverlayPersistentSettings timestampSettings = state.exportMediaPersistentSettings.timestampOverlay;

    state = parseMap(std::move(state), v, {
        { "format", [&](State state, const QVariant& v){
            static const QMap<QString, Qt::DateFormat> textToformat
            {
                {"Long", Qt::DefaultLocaleLongDate},
                {"Short", Qt::DefaultLocaleShortDate},
                {"ISO 8601", Qt::ISODate},
                {"RFC 2822", Qt::RFC2822Date}
            };
            timestampSettings.format = textToformat.value(v.toString(), Qt::DefaultLocaleLongDate);
            return state;
        } },
        { "fontSize", [&](State state, const QVariant& v){ timestampSettings.fontSize = v.toInt(); return state; } }
    });

    state = Reducer::setTimestampOverlaySettings(std::move(state), timestampSettings);

    return selectOverlayWithCoords(std::move(state), v, ExportOverlayType::timestamp);
}

} // namespace

std::pair<bool, State> ExportSettingsDialogStateReducer::applySettings(State state, QVariant settings)
{
    if (settings.isNull())
        return {false, std::move(state)};

    auto parseMedia = [&](State state, const QVariant& v){
        state = setMode(std::move(state), ExportSettingsDialog::Mode::Media).second;
        state = parseMap(std::move(state), v, {
            { "applyFilters", [](State state, const QVariant& v){ return setApplyFilters(std::move(state), v.toBool()).second; } },
            { "rapidReview", [](State state, const QVariant& v){
                return parseMap(std::move(state), v, {
                    { "speed", [](State state, const QVariant& v){
                        state = selectRapidReview(std::move(state));
                        return setSpeed(std::move(state), v.toInt()); }}
                });
            } },
            { "overlays", [&](State state, const QVariant& v){
                return parseList(std::move(state), v, "type", {
                    { "image", parseImageOverlay },
                    { "timestamp", parseTimestampOverlay },
                    { "text", parseTextOverlay },
                    { "bookmark", parseBookmarkOverlay }
                });
            } }
        });
        return state;
    };

    auto parseLayout = [](State state, const QVariant& v){
        state = setMode(std::move(state), ExportSettingsDialog::Mode::Layout).second;
        return parseMap(std::move(state), v, {
            { "readOnly", [](State state, const QVariant& v){ return setLayoutReadOnly(std::move(state), v.toBool()); } },
            { "password", [](State state, const QVariant& v){ return setLayoutEncryption(std::move(state), true, v.toString()); } }
        });
    };

    state = parseMap(std::move(state), settings,
    {
        { "media", parseMedia },
        { "layout", parseLayout },
        { "fileName", [](State state, const QVariant& v){
            auto filename = Filename::parse(v.toString());
            state = setMediaFilename(std::move(state), filename);
            return setLayoutFilename(std::move(state), filename);
        } }
    });

    return {true, std::move(state)};
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
    State state, Qt::DateFormat format)
{
    state.exportMediaPersistentSettings.timestampOverlay.format = format;
    return updateMaximumFontSizeTimestamp(std::move(state));
}

} // nx::vms::client::desktop
