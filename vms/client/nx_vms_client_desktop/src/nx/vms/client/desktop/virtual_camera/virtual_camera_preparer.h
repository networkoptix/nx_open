// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

#include "virtual_camera_fwd.h"

struct QnVirtualCameraPrepareReply;
class QnTimePeriodList;

namespace nx::vms::client::desktop {

class VirtualCameraPreparer:
    public QObject,
    public core::RemoteConnectionAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    VirtualCameraPreparer(const QnSecurityCamResourcePtr& camera, QObject* parent = nullptr);
    virtual ~VirtualCameraPreparer() override;

    void prepareUploads(const QStringList& filePaths, const QnTimePeriodList& queue);

private:
    void checkLocally(VirtualCameraPayload& payload);
    void handlePrepareFinished(bool success, const QnVirtualCameraPrepareReply& reply);

signals:
    void finished(const VirtualCameraUpload& result);
    void finishedLater(const VirtualCameraUpload& result);

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
