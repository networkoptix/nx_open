#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <api/model/api_model_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>
#include <nx/utils/uuid.h>

class QnNewWearableCameraDialog;

class QnWorkbenchWearableHandler : public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    explicit QnWorkbenchWearableHandler(QObject *parent = 0);
    ~QnWorkbenchWearableHandler();

private:
    void maybeOpenSettings();

private slots:
    void at_newWearableCameraAction_triggered();
    void at_uploadWearableCameraFileAction_triggered();

    void at_addWearableCameraAsync_finished(int status, const QnWearableCameraReply &reply, int handle);
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);

    void at_uploadWearableCameraFileAsync_finished(int status, int handle);

private:
    QPointer<QnNewWearableCameraDialog> m_dialog;
    QnUuid m_uuid;
};

