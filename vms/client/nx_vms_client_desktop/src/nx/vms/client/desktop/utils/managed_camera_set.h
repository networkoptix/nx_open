#pragma once

#include <functional>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

class QnResourcePool;

namespace nx::vms::client::desktop {

/**
 * This helper class allows to handle single/multiple/all camera selection
 * with specified filter automatically applied to the list.
 */
class ManagedCameraSet: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    /** A filter to apply to camera list. It must be immutable. */
    using Filter = std::function<bool(const QnVirtualCameraResourcePtr&)>;

    ManagedCameraSet(QnResourcePool* resourcePool,
        Filter filter = Filter(),
        QObject* parent = nullptr);

    virtual ~ManagedCameraSet() override = default;

    enum class Type
    {
        all,
        single,
        multiple
    };

    Type type() const;
    QnVirtualCameraResourceSet cameras() const;

    /** Selects all cameras in the resource pool that pass the filter. */
    void setAllCameras();

    /** Selects single specified camera if it passes the filter. */
    void setSingleCamera(const QnVirtualCameraResourcePtr& camera);

    /** Selects multiple cameras from specified set that pass the filter. */
    void setMultipleCameras(const QnVirtualCameraResourceSet& cameras);

    /**
     * Selected single camera or null if current set type is not Type::single,
     * if selected camera does not pass the filter or
     * if previously selected single camera was removed from the resource pool.
     */
    QnVirtualCameraResourcePtr singleCamera() const;

    Filter filter() const;

signals:
    /**
     * These signals are sent when current selection type or actual camera set is changed.
     */
    void camerasAboutToBeChanged(QPrivateSignal);
    void camerasChanged(QPrivateSignal);

    /**
     * These signals are sent when a camera is added to or removed from the resource pool
     * and it affects currently maintained camera set.
     */
    void cameraAdded(const QnVirtualCameraResourcePtr& camera, QPrivateSignal);
    void cameraRemoved(const QnVirtualCameraResourcePtr& camera, QPrivateSignal);

private:
    void setCameras(Type type, const QnVirtualCameraResourceSet& cameras);
    QnVirtualCameraResourceSet allCameras() const; //< All cameras from the pool, filtered.

    void addCamera(const QnVirtualCameraResourcePtr& camera);
    void removeCamera(const QnVirtualCameraResourcePtr& camera);

private:
    const QnResourcePool* m_resourcePool;
    const Filter m_filter;
    Type m_type = Type::multiple;
    QnVirtualCameraResourceSet m_cameras;
};

} // namespace nx::vms::client::desktop
