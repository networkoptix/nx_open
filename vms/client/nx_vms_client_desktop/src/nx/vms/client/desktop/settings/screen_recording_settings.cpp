// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "screen_recording_settings.h"

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtCore/QDir>

#include <nx/utils/log/assert.h>
#include <nx/utils/property_storage/qsettings_migration_utils.h>
#include <nx/vms/client/desktop/application_context.h>

namespace nx::vms::client::desktop {

using namespace screen_recording;

ScreenRecordingSettings::ScreenRecordingSettings()
{
    load();
    migrateFrom_v51();

    if (recordingFolder().isEmpty())
        recordingFolder = QDir::tempPath();
}

int ScreenRecordingSettings::screen() const
{
    const int savedScreen = screenProperty();
    const QRect geometry = screenGeometry();

    const auto screens = QGuiApplication::screens();
    if (NX_ASSERT(savedScreen >= 0) && savedScreen < screens.size()
        && screens.at(savedScreen)->geometry() == geometry)
    {
        return savedScreen;
    }

    for (int i = 0; i < screens.size(); i++)
    {
        if (screens.at(i)->geometry() == geometry)
            return i;
    }

    return screens.indexOf(QGuiApplication::primaryScreen());
}

void ScreenRecordingSettings::setScreen(int screen)
{
    const auto screens = QGuiApplication::screens();
    if (!NX_ASSERT(screen >= 0 && screen < screens.size()))
        return;

    screenProperty = screen;
    screenGeometry = screens.at(screen)->geometry();
}

void ScreenRecordingSettings::migrateFrom_v51()
{
    if (migrationDone())
        return;

    using namespace nx::utils::property_storage;

    const QString kPrefix = "videoRecording/";

    auto oldSettings = std::make_unique<QSettings>();

    migrateValue(oldSettings.get(), primaryAudioDeviceName, kPrefix + primaryAudioDeviceName.name);
    migrateValue(oldSettings.get(), secondaryAudioDeviceName,
        kPrefix + secondaryAudioDeviceName.name);

    // Video recording settings were stored in the General group.
    migrateValue(oldSettings.get(), captureCursor);
    migrateEnumValue(oldSettings.get(), captureMode);
    if (captureMode() == CaptureMode::window)
        captureMode = CaptureMode::fullScreen; //< Window mode support was removed.
    migrateEnumValue(oldSettings.get(), quality, "decoderQuality");
    migrateEnumValue(oldSettings.get(), resolution);
    migrateValue(oldSettings.get(), screenProperty);
    migrateValue(oldSettings.get(), screenGeometry, "screenResolution");
    migrateValue(oldSettings.get(), recordingFolder, kPrefix + recordingFolder.name);

    migrationDone = true;
}

ScreenRecordingSettings* screenRecordingSettings()
{
    return appContext()->screenRecordingSettings();
}

} // namespace nx::vms::client::desktop
