#pragma once

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

#include <core/resource/resource_fwd.h>
#include <api/model/api_model_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>
#include <nx/vms/client/desktop/utils/wearable_fwd.h>

class QnWorkbenchWearableHandler:
    public Connective<QObject>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    explicit QnWorkbenchWearableHandler(QObject* parent = nullptr);
    virtual ~QnWorkbenchWearableHandler() override;

private:
    void maybeOpenCurrentSettings();
    bool fixFileUpload(
        const QnSecurityCamResourcePtr& camera,
        nx::vms::client::desktop::WearableUpload *upload);
    bool fixFolderUpload(
        const QString& path, 
        const QnSecurityCamResourcePtr& camera, 
        nx::vms::client::desktop::WearableUpload* upload);
    bool fixStorageCleanupUpload(nx::vms::client::desktop::WearableUpload* upload);
    void uploadValidFiles(
        const QnSecurityCamResourcePtr& camera,
        const nx::vms::client::desktop::WearablePayloadList& payloads);

private slots:
    void at_newWearableCameraAction_triggered();
    void at_uploadWearableCameraFileAction_triggered();
    void at_uploadWearableCameraFolderAction_triggered();
    void at_cancelWearableCameraUploadsAction_triggered();
    void at_resourcePool_resourceAdded(const QnResourcePtr& resource);
    void at_context_userChanged();
    void at_wearableManager_stateChanged(const nx::vms::client::desktop::WearableState& state);

private:
    QString calculateExtendedErrorMessage(const nx::vms::client::desktop::WearablePayload& upload);
    QnSecurityCamResourcePtr cameraByProgressId(const QnUuid& progressId) const;
    QnUuid ensureProgress(const QnUuid& cameraId);
    void removeProgress(const QnUuid& cameraId);

private:
    QnUuid m_currentCameraUuid;
    QHash<QnUuid, QnUuid> m_currentProgresses; //< Camera id -> progress id.
};

