// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtQuickWidgets/QQuickWidget>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class CameraAnalyticsSettingsWidget: public QQuickWidget
{
    using base_type = QQuickWidget;

public:
    CameraAnalyticsSettingsWidget(
        CameraSettingsDialogStore* store,
        QQmlEngine* engine,
        QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop
