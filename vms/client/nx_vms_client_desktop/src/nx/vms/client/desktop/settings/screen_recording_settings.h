// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/serialization/qt_geometry_reflect_json.h>
#include <nx/vms/client/core/settings/audio_recording_settings.h>
#include <nx/vms/client/desktop/resource/screen_recording/types.h>

namespace nx::vms::client::desktop {

class ScreenRecordingSettings: public core::AudioRecordingSettings
{
public:
    ScreenRecordingSettings();

    Property<bool> captureCursor{this, "captureCursor", true};
    Property<screen_recording::CaptureMode> captureMode{this, "captureMode"};
    Property<screen_recording::Quality> quality{
        this, "quality", screen_recording::Quality::balanced};
    Property<screen_recording::Resolution> resolution{
        this, "resolution", screen_recording::Resolution::quarterNative};
    Property<QString> recordingFolder{this, "recordingFolder"};

    int screen() const;
    void setScreen(int screen);

private:
    Property<int> screenProperty{this, "screen"};
    Property<QRect> screenGeometry{this, "screenGeometry"};

    /** Flag whether settings were successfully migrated from the 5.1 settings format. */
    Property<bool> migrationDone{this, "migrationDone"};

    void migrateFrom_v51();
};

ScreenRecordingSettings* screenRecordingSettings();

} // namespace nx::vms::client::desktop
