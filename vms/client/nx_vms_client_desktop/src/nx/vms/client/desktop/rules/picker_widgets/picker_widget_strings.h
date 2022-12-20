// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <core/resource/device_dependent_strings.h>
#include <nx/vms/api/types/event_rule_types.h>

namespace nx::vms::client::desktop::rules {

class CommonPickerWidgetStrings
{
    Q_DECLARE_TR_FUNCTIONS(SourcePickerWidgetStrings)

public:
    static QString testButtonDisplayText();
};

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

class FlagsPickerWidgetStrings
{
    Q_DECLARE_TR_FUNCTIONS(FlagsPickerWidgetStrings)

public:
    static QString eventLevelDisplayString(api::EventLevel eventLevel);
};

} // namespace nx::vms::client::desktop::rules
