// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <core/resource/device_dependent_strings.h>

namespace nx::vms::client::desktop::rules {

class SourcePickerWidgetStrings
{
    Q_DECLARE_TR_FUNCTIONS(SourcePickerWidgetStrings)

public:
    static QString selectServer();
    static QString selectUser();
    static QString selectDevice(QnCameraDeviceType deviceType);
};

class DropdownTextPickerWidgetStrings
{
    Q_DECLARE_TR_FUNCTIONS(DropdownTextPickerWidgetStrings)

public:
    static QString autoValue();
};

} // namespace nx::vms::client::desktop::rules
