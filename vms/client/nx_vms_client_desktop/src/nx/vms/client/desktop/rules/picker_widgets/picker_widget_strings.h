// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <core/resource/device_dependent_strings.h>
#include <nx/vms/api/rules/common.h>
#include <nx/vms/api/types/event_rule_types.h>

namespace nx::vms::client::desktop::rules {

class CommonPickerWidgetStrings
{
    Q_DECLARE_TR_FUNCTIONS(SourcePickerWidgetStrings)

public:
    static QString testButtonDisplayText();
};

class ResourcePickerWidgetStrings
{
    Q_DECLARE_TR_FUNCTIONS(SourcePickerWidgetStrings)

public:
    static QString selectServer();
    static QString selectUser();
    static QString selectDevice(QnCameraDeviceType deviceType);
    static QString useEventSourceString(const QString& actionType = {});
};

class DropdownTextPickerWidgetStrings
{
    Q_DECLARE_TR_FUNCTIONS(DropdownTextPickerWidgetStrings)

public:
    static QString autoValue();
    static QString automaticValue();
};

class FlagsPickerWidgetStrings
{
    Q_DECLARE_TR_FUNCTIONS(FlagsPickerWidgetStrings)

public:
    static QString eventLevelDisplayString(api::EventLevel eventLevel);
};

class DurationPickerWidgetStrings
{
    Q_DECLARE_TR_FUNCTIONS(DurationPickerWidgetStrings)

public:
    static QString intervalOfActionHint(bool isInstant);
    static QString playbackTimeHint(bool isLive);
};

class CameraPickerStrings
{
    Q_DECLARE_TR_FUNCTIONS(CameraPickerStrings)

public:
    static QString sourceCameraString(size_t otherCameraCount = 0);
};

class ServerPickerStrings
{
    Q_DECLARE_TR_FUNCTIONS(ServerPickerStrings)

public:
    static QString anyServerString();
    static QString multipleServersString(size_t count);
    static QString selectServerString();
    static QString sourceServerString(size_t otherServersCount);
};

} // namespace nx::vms::client::desktop::rules
