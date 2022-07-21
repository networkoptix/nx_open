// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QWidget>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

class CameraSettingsDialogStore;
class LiveCameraThumbnail;

class CameraHotspotsSettingsWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    CameraHotspotsSettingsWidget(
        CameraSettingsDialogStore* store,
        const QSharedPointer<LiveCameraThumbnail>& cameraThumbnail,
        QWidget* parent = nullptr);

    virtual ~CameraHotspotsSettingsWidget() override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
