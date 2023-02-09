// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picker_widget_strings.h"

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

QString ResourcePickerWidgetStrings::useEventSourceString()
{
    return tr("Use event source camera");
}

QString DropdownTextPickerWidgetStrings::autoValue()
{
    return tr("Auto");
}

QString DropdownTextPickerWidgetStrings::state(api::rules::State value)
{
    switch (value)
    {
        case api::rules::State::instant:
            return tr("Occurs");
        case api::rules::State::started:
            return tr("Starts");
        case api::rules::State::stopped:
            return tr("Stops");
        default:
            return tr("None");
    }
}

QString FlagsPickerWidgetStrings::eventLevelDisplayString(api::EventLevel eventLevel)
{
    switch(eventLevel)
    {
        case api::EventLevel::ErrorEventLevel:
            return tr("Error");
        case api::EventLevel::WarningEventLevel:
            return tr("Warning");
        case api::EventLevel::InfoEventLevel:
            return tr("Info");
        default:
            return tr("Undefined");
    }
}

QString DurationPickerWidgetStrings::intervalOfActionHint(bool isInstant)
{
    return isInstant
        ? tr("Interval of action: Instant")
        : tr("Interval of action: No more than once per");
}

QString CameraPickerStrings::sourceCameraString(size_t otherCameraCount)
{
    if (otherCameraCount == 0)
        return tr("Source Camera");

    return tr("Source and %n more Cameras", "", otherCameraCount);
}

QString ServerPickerStrings::anyServerString()
{
    return QString{"<%n>"}.arg(tr("Any Server"));
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
