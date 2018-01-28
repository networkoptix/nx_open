#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <api/model/api_model_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>
#include <nx/utils/uuid.h>

class QnNewWearableCameraDialog;
namespace nx { namespace client { namespace desktop { struct UploadState; }}}

class QnWorkbenchWearableHandler: public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    explicit QnWorkbenchWearableHandler(QObject* parent = nullptr);
    virtual ~QnWorkbenchWearableHandler() override;

private:
    qreal calculateProgress(const nx::client::desktop::UploadState& upload, bool processed);

    void maybeOpenCurrentSettings();

private slots:
    void at_newWearableCameraAction_triggered();
    void at_uploadWearableCameraFileAction_triggered();

    void at_resourcePool_resourceAdded(const QnResourcePtr& resource);

    void at_upload_progress(const nx::client::desktop::UploadState& upload);

private:
    struct FootageInfo
    {
        QnSecurityCamResourcePtr camera;
        qint64 startTimeMs;
    };

    QnUuid m_currentCameraUuid;
    QHash<QString, FootageInfo> m_infoByUploadId;
};

