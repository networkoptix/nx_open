// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../export_settings_dialog.h"

#include <QtCore/QSize>
#include <QtCore/QString>
#include <QtCore/QDir>

#include <nx/vms/client/desktop/common/flux/abstract_flux_state.h>
#include <nx/vms/client/desktop/export/settings/export_layout_persistent_settings.h>
#include <nx/vms/client/desktop/export/settings/export_media_persistent_settings.h>

class QnClientSettings;

namespace nx::vms::client::desktop {

struct NX_VMS_CLIENT_DESKTOP_API ExportSettingsDialogState: public AbstractFluxState
{
    QSize previewSize;
    QString bookmarkName;
    QString bookmarkDescription;

    ExportSettingsDialog::Mode mode = ExportSettingsDialog::Mode::Media;

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

    static State setTimestampOffsetMs(State state, qint64 offsetMs);

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

    static State setServerTimeZoneOffsetMs(State state, qint64 offsetMs);

    static State loadSettings(State state, const QnClientSettings* settings, const QString& cacheDirLocation);

    static State setLayoutReadOnly(State state, bool value);

    static State setLayoutEncryption(State state, bool on, const QString& password);

    static State setBookmarks(State state, const QnCameraBookmarkList& bookmarks);

    static State setMediaResourceSettings(State state, bool hasVideo, const nx::core::transcoding::Settings& settings);

    static std::pair<bool, State> setFrameSize(State state, const QSize& size);

    static State setTimePeriod(State state, const QnTimePeriod& period);

    static State setMediaFilename(State state, const Filename& filename);

    static State setLayoutFilename(State state, const Filename& filename);

    static std::pair<bool, State> setMode(State state, ExportSettingsDialog::Mode mode);

    static State enableTab(State state, ExportSettingsDialog::Mode mode);

    static State disableTab(State state, ExportSettingsDialog::Mode mode, const QString& reason);

    static State setSpeed(State state, int speed);

    static std::pair<bool, State> applySettings(State state, QVariant settings);

    static State setTimestampFont(State state, const QFont& font);

    static State updateMaximumFontSizeTimestamp(State state);

    static State setFormatTimestampOverlay(State state, Qt::DateFormat format);
};

} // namespace nx::vms::client::desktop
