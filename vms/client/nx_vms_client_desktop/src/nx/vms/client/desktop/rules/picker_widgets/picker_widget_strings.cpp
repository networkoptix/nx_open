// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "picker_widget_strings.h"

namespace nx::vms::client::desktop::rules {

QString SourcePickerWidgetStrings::selectServer()
{
    return tr("Select at least one Server");
}

QString SourcePickerWidgetStrings::selectUser()
{
    return tr("Select at least one user");
}

QString SourcePickerWidgetStrings::selectDevice(QnCameraDeviceType deviceType)
{
    static const QnCameraDeviceStringSet deviceStringSet{
        tr("Select at least one device"),
        tr("Select at least one camera"),
        tr("Select at least one I/O module")
    };
    return deviceStringSet.getString(deviceType);
}

QString DropdownTextPickerWidgetStrings::autoValue()
{
    return tr("Auto");
}

} // namespace nx::vms::client::desktop::rules
