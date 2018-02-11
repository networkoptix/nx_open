#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <api/model/api_model_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>
#include <nx/utils/uuid.h>
#include <nx/client/desktop/utils/wearable_payload.h>

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
    bool checkFileUpload(const nx::client::desktop::WearablePayloadList& uploads);
    bool checkFolderUpload(const QString& path, const nx::client::desktop::WearablePayloadList& uploads);
    void uploadValidFiles(
        const QnSecurityCamResourcePtr& camera,
        const nx::client::desktop::WearablePayloadList& uploads);

private slots:
    void at_newWearableCameraAction_triggered();
    void at_uploadWearableCameraFileAction_triggered();
    void at_uploadWearableCameraFolderAction_triggered();
    void at_resourcePool_resourceAdded(const QnResourcePtr& resource);
    void at_context_userChanged();

private:
    QString calculateExtendedErrorMessage(const nx::client::desktop::WearablePayload& upload);

private:
    QnUuid m_currentCameraUuid;
};

