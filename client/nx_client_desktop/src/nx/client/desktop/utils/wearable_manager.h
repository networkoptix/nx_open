#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <common/common_module_aware.h>
#include <nx/utils/uuid.h>

#include "wearable_state.h"
#include "wearable_payload.h"

namespace nx {
namespace client {
namespace desktop {

class WearableWorker;

class WearableManager:
    public QObject,
    public QnCommonModuleAware
{
    Q_OBJECT
public:
    WearableManager(QObject* parent = nullptr);
    virtual ~WearableManager() override;

    void setCurrentUser(const QnUserResourcePtr& user);
    const QnUserResourcePtr& currentUser() const;

    WearableState state(const QnSecurityCamResourcePtr& camera);
    QList<WearableState> runningUploads();

    void updateState(const QnSecurityCamResourcePtr& camera);
    WearablePayloadList checkUploads(const QnSecurityCamResourcePtr& camera, const QStringList& filePaths);
    bool addUploads(const QnSecurityCamResourcePtr& camera, const WearablePayloadList& uploads);
    void cancelUploads(const QnSecurityCamResourcePtr& camera);
    void cancelAllUploads();

signals:
    void stateChanged(const WearableState& state);
    void error(const WearableState& state, const QString& errorMessage);

private:
    WearableWorker* cameraWorker(const QnSecurityCamResourcePtr& camera);
    void dropWorker(const QnResourcePtr& resource);
    void dropAllWorkers();

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
