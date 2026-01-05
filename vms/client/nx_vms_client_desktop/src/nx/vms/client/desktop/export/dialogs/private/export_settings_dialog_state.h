// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QDir>
#include <QtCore/QSize>
#include <QtCore/QString>

#include <nx/vms/client/desktop/common/flux/abstract_flux_state.h>
#include <nx/vms/client/desktop/export/settings/export_layout_persistent_settings.h>
#include <nx/vms/client/desktop/export/settings/export_media_persistent_settings.h>

#include "../export_settings_dialog.h"

namespace nx::vms::client::desktop {

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
    api::PixelationSettings systemPixelationSettings = {};

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

} // namespace nx::vms::client::desktop
