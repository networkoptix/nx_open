// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDir>
#include <QtCore/QSize>
#include <QtCore/QString>

#include <nx/core/transcoding/timestamp_format.h>
#include <nx/vms/client/desktop/common/flux/abstract_flux_state.h>
#include <nx/vms/client/desktop/export/settings/export_layout_persistent_settings.h>
#include <nx/vms/client/desktop/export/settings/export_media_persistent_settings.h>

#include "../export_settings_dialog.h"

namespace nx::vms::client::desktop {

class LocalSettings;

struct NX_VMS_CLIENT_DESKTOP_API ExportSettingsDialogState: public AbstractFluxState
{
    QSize previewSize;
    QString bookmarkName;
    QString bookmarkDescription;

    ExportMode mode = ExportMode::media;

    ExportMediaSettings exportMediaSettings;
    ExportMediaPersistentSettings exportMediaPersistentSettings;

    ExportLayoutSettings exportLayoutSettings;
    ExportLayoutPersistentSettings exportLayoutPersistentSettings;

    ExportOverlayType selectedOverlayType = ExportOverlayType::none;
    bool rapidReviewSelected = false;

    nx::core::transcoding::Settings availableTranscodingSettings;

    QSize fullFrameSize;
    qreal overlayScale = 1.0;

    QString mediaDisableReason;
    QString layoutDisableReason;

    bool mediaAvailable = false;
    bool layoutAvailable = false;

    QDir cacheDir;

    ExportSettingsDialogState(QSize previewSize, QString bookmarkName, QString bookmarkDescription) :
        previewSize(previewSize), bookmarkName(bookmarkName), bookmarkDescription(bookmarkDescription)
    {
        exportLayoutSettings.mode = ExportLayoutSettings::Mode::Export;
        updateBookmarkText();
    }

    void setCacheDirLocation(const QString& location);

    QString cachedImageFileName() const;

    ExportMediaSettings getExportMediaSettings() const
    {
        auto runtimeSettings = exportMediaSettings;
        exportMediaPersistentSettings.updateRuntimeSettings(runtimeSettings);
        return runtimeSettings;
    }

    ExportLayoutSettings getExportLayoutSettings() const
    {
        auto runtimeSettings = exportLayoutSettings;
        exportLayoutPersistentSettings.updateRuntimeSettings(runtimeSettings);
        return runtimeSettings;
    }

    void applyTranscodingSettings();

    void updateBookmarkText();
};

class NX_VMS_CLIENT_DESKTOP_API ExportSettingsDialogStateReducer
{
public:

    using State = ExportSettingsDialogState;

    static State setWatermark(State state, const nx::core::Watermark& watermark)
    {
        state.exportMediaSettings.transcodingSettings.watermark = watermark;
        state.exportLayoutSettings.watermark = watermark;
        return state;
    }

    static State selectOverlay(State state, ExportOverlayType type);

    static State hideOverlay(State state, ExportOverlayType type);

    static State selectRapidReview(State state);

    static State clearSelection(State state);

    static State hideRapidReview(State state);

    static std::pair<bool, State> setApplyFilters(State state, bool value);

    static State setTimestampTimeZone(State state, const QTimeZone& timeZone);

    static State setTimestampOverlaySettings(
        State state,
        const ExportTimestampOverlayPersistentSettings& settings);

    static State setImageOverlaySettings(
        State state,
        const ExportImageOverlayPersistentSettings& settings);

    static State setTextOverlaySettings(
        State state,
        const ExportTextOverlayPersistentSettings& settings);

    static State setBookmarkOverlaySettings(
        State state,
        const ExportBookmarkOverlayPersistentSettings& settings);

    static State setInfoOverlaySettings(
        State state,
        const ExportInfoOverlayPersistentSettings& settings);

    static std::pair<bool, State> setOverlayOffset(State state, ExportOverlayType type, int x, int y);

    static std::pair<bool, State> overlayPositionChanged(State state, ExportOverlayType type, QRect geometry, QSize space);

    static State loadSettings(State state,
        const LocalSettings* localSettings,
        const QString& cacheDirLocation);

    static State setLayoutReadOnly(State state, bool value);

    static State setLayoutEncryption(State state, bool on, const QString& password);

    static State setBookmarks(State state, const QnCameraBookmarkList& bookmarks);

    static State setMediaResourceSettings(State state, bool hasVideo, const nx::core::transcoding::Settings& settings);

    static std::pair<bool, State> setFrameSize(State state, const QSize& size);

    static State setTimePeriod(State state, const QnTimePeriod& period);

    static State setMediaFilename(State state, const Filename& filename);

    static State setLayoutFilename(State state, const Filename& filename);

    static std::pair<bool, State> setMode(State state, ExportMode mode);

    static State enableTab(State state, ExportMode mode);

    static State disableTab(State state, ExportMode mode, const QString& reason);

    static State setSpeed(State state, int speed);

    static State setTimestampFont(State state, const QFont& font);

    static State updateMaximumFontSizeTimestamp(State state);

    static State setFormatTimestampOverlay(
        State state,
        nx::core::transcoding::TimestampFormat format);
};

} // namespace nx::vms::client::desktop
