// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <api/model/api_model_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/window_context_aware.h>

#include "virtual_camera_fwd.h"

namespace nx::vms::client::desktop {

class VirtualCameraActionHandler:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit VirtualCameraActionHandler(WindowContext* windowContext, QObject* parent = nullptr);
    virtual ~VirtualCameraActionHandler() override;

private:
    void maybeOpenCurrentSettings();
    bool fixFileUpload(
        const QnSecurityCamResourcePtr& camera,
        nx::vms::client::desktop::VirtualCameraUpload *upload);
    bool fixFolderUpload(
        const QString& path,
        const QnSecurityCamResourcePtr& camera,
        nx::vms::client::desktop::VirtualCameraUpload* upload);
    bool fixStorageCleanupUpload(nx::vms::client::desktop::VirtualCameraUpload* upload);
    void uploadValidFiles(
        const QnSecurityCamResourcePtr& camera,
        const nx::vms::client::desktop::VirtualCameraPayloadList& payloads);

private slots:
    void at_newVirtualCameraAction_triggered();
    void at_uploadVirtualCameraFileAction_triggered();
    void at_uploadVirtualCameraFolderAction_triggered();
    void at_cancelVirtualCameraUploadsAction_triggered();
    void at_resourcePool_resourceAdded(const QnResourcePtr& resource);
    void at_virtualCameraManager_stateChanged(const nx::vms::client::desktop::VirtualCameraState& state);

private:
    QString calculateExtendedErrorMessage(const nx::vms::client::desktop::VirtualCameraPayload& upload);
    QnSecurityCamResourcePtr cameraByProgressId(const QnUuid& progressId) const;
    QnUuid ensureProgress(const QnUuid& cameraId);
    void removeProgress(const QnUuid& cameraId);

private:
    QnUuid m_currentCameraUuid;
    QHash<QnUuid, QnUuid> m_currentProgresses; //< Camera id -> progress id.
};

} // namespace nx::vms::client::desktop
