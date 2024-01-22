// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picker_widget_strings.h"

#include <nx/vms/rules/actions/bookmark_action.h>
#include <nx/vms/rules/actions/device_output_action.h>
#include <nx/vms/rules/actions/device_recording_action.h>
#include <nx/vms/rules/actions/enter_fullscreen_action.h>
#include <nx/vms/rules/actions/play_sound_action.h>
#include <nx/vms/rules/actions/repeat_sound_action.h>
#include <nx/vms/rules/actions/show_on_alarm_layout_action.h>
#include <nx/vms/rules/actions/speak_action.h>
#include <nx/vms/rules/actions/text_overlay_action.h>
#include <nx/vms/rules/utils/type.h>

namespace nx::vms::client::desktop::rules {

QString CommonPickerWidgetStrings::testButtonDisplayText()
{
    return tr("Test");
}

QString ResourcePickerWidgetStrings::selectServer()
{
    return tr("Select at least one Server");
}

QString ResourcePickerWidgetStrings::selectUser()
{
    return tr("Select at least one user");
}

QString ResourcePickerWidgetStrings::selectDevice(QnCameraDeviceType deviceType)
{
    static const QnCameraDeviceStringSet deviceStringSet{
        tr("Select at least one device"),
        tr("Select at least one camera"),
        tr("Select at least one I/O module")
    };
    return deviceStringSet.getString(deviceType);
}

QString ResourcePickerWidgetStrings::useEventSourceString(const QString& actionType)
{
    // TODO: #mmalofeev consider move to the manifest.

    if (!actionType.isEmpty())
    {
        if (vms::rules::utils::type<vms::rules::BookmarkAction>() == actionType)
            return tr("Also set on source camera");

        if (vms::rules::utils::type<vms::rules::DeviceOutputAction>() == actionType)
            return tr("Also trigger on source camera");

        if (vms::rules::utils::type<vms::rules::DeviceRecordingAction>() == actionType)
            return tr("Also record source camera");

        if (vms::rules::utils::type<vms::rules::EnterFullscreenAction>() == actionType)
            return tr("Source camera");

        if (vms::rules::utils::type<vms::rules::PlaySoundAction>() == actionType
            || vms::rules::utils::type<vms::rules::RepeatSoundAction>() == actionType
            || vms::rules::utils::type<vms::rules::SpeakAction>() == actionType)
        {
            return tr("Also play on source camera");
        }

        if (vms::rules::utils::type<vms::rules::ShowOnAlarmLayoutAction>() == actionType)
            return tr("Also show source camera");

        if (vms::rules::utils::type<vms::rules::TextOverlayAction>() == actionType)
            return tr("Also show on source camera");
    }

    return tr("Use event source camera");
}

QString DropdownTextPickerWidgetStrings::autoValue()
{
    return tr("Auto");
}

QString DropdownTextPickerWidgetStrings::automaticValue()
{
    return tr("automatic");
}

QString FlagsPickerWidgetStrings::eventLevelDisplayString(api::EventLevel eventLevel)
{
    switch(eventLevel)
    {
        case api::EventLevel::error:
            return tr("Error");
        case api::EventLevel::warning:
            return tr("Warning");
        case api::EventLevel::info:
            return tr("Info");
        default:
            return tr("Undefined");
    }
}

QString DurationPickerWidgetStrings::intervalOfActionHint(bool isInstant)
{
    return isInstant ? tr("Instant") : tr("No more than once per");
}

QString DurationPickerWidgetStrings::playbackTimeHint(bool isLive)
{
    return isLive ? tr("Live") : tr("Rewind for");
}

QString CameraPickerStrings::sourceCameraString(size_t otherCameraCount)
{
    if (otherCameraCount == 0)
        return tr("Source Camera");

    return tr("Source and %n more Cameras", "", otherCameraCount);
}

QString ServerPickerStrings::anyServerString()
{
    return QString{"<%1>"}.arg(tr("Any Server"));
}

QString ServerPickerStrings::multipleServersString(size_t count)
{
    return tr("%n Servers", "", count);
}

QString ServerPickerStrings::selectServerString()
{
    return tr("Select Server");
}

QString ServerPickerStrings::sourceServerString(size_t otherServersCount)
{
    if (otherServersCount == 0)
        return tr("Source Server");

    return tr("Source Server and %n Servers", "", otherServersCount);
}

} // namespace nx::vms::client::desktop::rules
