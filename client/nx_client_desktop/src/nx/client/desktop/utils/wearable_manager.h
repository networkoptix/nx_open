#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

#include "wearable_state.h"

namespace nx {
namespace client {
namespace desktop {

class WearableWorker;

class WearableManager: public QObject
{
    Q_OBJECT
public:
    WearableManager(QObject* parent = nullptr);
    virtual ~WearableManager() override;

    void setCurrentUser(const QnUserResourcePtr& user);
    const QnUserResourcePtr& currentUser() const;

    WearableState state(const QnSecurityCamResourcePtr& camera);

    void updateState(const QnSecurityCamResourcePtr& camera);
    bool addUpload(const QnSecurityCamResourcePtr& camera, const QString& path, QString* errorMessage);

signals:
    void stateChanged(const WearableState& state);
    void error(const WearableState& state, const QString& errorMessage);

private:
    WearableWorker* ensureWorker(const QnSecurityCamResourcePtr& camera);
    void clearAllWorkers();

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace desktop
} // namespace client
} // namespace nx
