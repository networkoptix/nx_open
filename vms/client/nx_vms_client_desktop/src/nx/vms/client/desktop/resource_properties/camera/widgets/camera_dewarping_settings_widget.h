// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>
#include <QtQuickWidgets/QQuickWidget>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;
class LiveCameraThumbnail;

class CameraDewarpingSettingsWidget: public QQuickWidget
{
    using base_type = QQuickWidget;

public:
    CameraDewarpingSettingsWidget(
        CameraSettingsDialogStore* store,
        QSharedPointer<LiveCameraThumbnail> thumbnail,
        QQmlEngine* engine,
        QWidget* parent = nullptr);

private:
    const QSharedPointer<LiveCameraThumbnail> m_thumbnail;
};

} // namespace nx::vms::client::desktop
