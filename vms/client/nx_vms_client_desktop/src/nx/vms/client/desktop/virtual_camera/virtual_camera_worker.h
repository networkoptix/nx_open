// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

#include "virtual_camera_fwd.h"
#include "virtual_camera_state.h"

struct QnVirtualCameraStatusReply;

namespace nx::vms::client::desktop {

struct UploadState;

class VirtualCameraWorker:
    public QObject,
    public core::CommonModuleAware,
    public core::RemoteConnectionAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    VirtualCameraWorker(
        const QnSecurityCamResourcePtr& camera,
        const QnUserResourcePtr& user,
        QObject* parent = nullptr);
    virtual ~VirtualCameraWorker() override;

    void updateState();
    bool addUpload(const VirtualCameraPayloadList& payloads);
    void cancel();

    VirtualCameraState state() const;

    bool isWorking() const;

signals:
    void stateChanged(const VirtualCameraState& state);
    void error(const VirtualCameraState& state, const QString &errorMessage);
    void finished();

private:
    void processCurrentFile();
    void processNextFile();

    VirtualCameraState::Status calculateNewStatus(VirtualCameraState::Error error);
    QString calculateErrorMessage(VirtualCameraState::Error error, const QString& errorMessage);
    void handleFailure(VirtualCameraState::Error error, const QString& errorMessage = QString());
    void handleStop();
    void handleStatusFinished(bool success, const QnVirtualCameraStatusReply& result);
    void handleLockFinished(bool success, const QnVirtualCameraStatusReply& result);
    void handleExtendFinished(bool success, const QnVirtualCameraStatusReply& result);
    void handleUnlockFinished(bool success, const QnVirtualCameraStatusReply& result);
    void handleUploadProgress(const UploadState& state);
    void handleConsumeStarted(bool success);

    void pollExtend();

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
