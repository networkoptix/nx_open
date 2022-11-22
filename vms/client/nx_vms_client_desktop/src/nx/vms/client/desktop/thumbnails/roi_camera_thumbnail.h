// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

#include "live_camera_thumbnail.h"

namespace nx::vms::client::desktop {

class RoiCameraThumbnail: public LiveCameraThumbnail
{
    Q_OBJECT
    using base_type = LiveCameraThumbnail;

    Q_PROPERTY(QnUuid cameraId READ cameraId WRITE setCameraId NOTIFY resourceChanged)

public:
    RoiCameraThumbnail(QObject* parent = nullptr);
    virtual ~RoiCameraThumbnail() override;

    QnUuid cameraId() const;
    void setCameraId(const QnUuid& value);

    static void registerQmlType();

signals:
    void engineIdChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
