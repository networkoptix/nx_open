#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <api/model/api_model_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>
#include <nx/utils/uuid.h>

class QnNewWearableCameraDialog;
struct QnFileUpload;

class QnWorkbenchWearableHandler: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    explicit QnWorkbenchWearableHandler(QObject *parent = 0);
    virtual ~QnWorkbenchWearableHandler() override;

private:
    void maybeOpenCurrentSettings();

private slots:
    void at_newWearableCameraAction_triggered();
    void at_uploadWearableCameraFileAction_triggered();

    void at_addWearableCameraAsync_finished(int status, const QnWearableCameraReply &reply, int handle);
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);

    void at_uploadManager_progress(const QnFileUpload& upload);
    void at_processWearableCameraFileAsync_finished(int status, int handle);

private:
    struct FootageInfo
    {
        QnSecurityCamResourcePtr camera;
        qint64 startTimeMs;
    };

    QPointer<QnNewWearableCameraDialog> m_dialog;
    QnUuid m_currentCameraUuid;
    QHash<QString, FootageInfo> m_infoByUploadId;
};

