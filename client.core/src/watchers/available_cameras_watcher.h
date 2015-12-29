#pragma once

#include <utils/common/id.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/connective.h>

class QnAvailableCamerasWatcherPrivate;
class QnAvailableCamerasWatcher : public Connective<QObject>
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnAvailableCamerasWatcher(QObject *parent = nullptr);
    ~QnAvailableCamerasWatcher();

    QnUserResourcePtr user() const;
    void setUser(const QnUserResourcePtr &user);

    bool isCameraAvailable(const QnUuid &cameraId) const;
    QnVirtualCameraResourceList availableCameras() const;

signals:
    void cameraAdded(const QnResourcePtr &resource);
    void cameraRemoved(const QnResourcePtr &resource);

private:
    QScopedPointer<QnAvailableCamerasWatcherPrivate> const d_ptr;
    Q_DECLARE_PRIVATE(QnAvailableCamerasWatcher)
};
