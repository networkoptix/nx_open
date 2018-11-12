#pragma once

#include <common/common_module_aware.h>

#include <nx/utils/uuid.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnAvailableCamerasWatcherPrivate;
class QnAvailableCamerasWatcher: public Connective<QObject>, public QnCommonModuleAware
{
    Q_OBJECT

    using base_type = Connective<QObject>;

public:
    QnAvailableCamerasWatcher(QObject* parent = nullptr);
    ~QnAvailableCamerasWatcher();

    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr& user);

    QnVirtualCameraResourceList availableCameras() const;

    bool compatibilityMode() const;
    void setCompatiblityMode(bool compatibilityMode);

signals:
    void cameraAdded(const QnResourcePtr& resource);
    void cameraRemoved(const QnResourcePtr& resource);

private:
    QScopedPointer<QnAvailableCamerasWatcherPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnAvailableCamerasWatcher)
};
