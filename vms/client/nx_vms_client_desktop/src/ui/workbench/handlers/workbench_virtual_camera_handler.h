// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

#include <core/resource/resource_fwd.h>
#include <api/model/api_model_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <nx/vms/client/desktop/utils/virtual_camera_fwd.h>

class QnWorkbenchVirtualCameraHandler:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit QnWorkbenchVirtualCameraHandler(QObject* parent = nullptr);
    virtual ~QnWorkbenchVirtualCameraHandler() override;

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
    void at_context_userChanged();
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

