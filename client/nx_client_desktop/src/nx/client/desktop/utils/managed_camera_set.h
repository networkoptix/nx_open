#pragma once

#include <functional>

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

class QnResourcePool;

namespace nx::client::desktop {

// TODO: #vkutin Document this.

class ManagedCameraSet: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
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
    QnVirtualCameraResourcePtr singleCamera() const; //< May return null.
    Filter filter() const;

    void setAllCameras();
    void setSingleCamera(const QnVirtualCameraResourcePtr& camera);
    void setMultipleCameras(const QnVirtualCameraResourceSet& cameras);

signals:
    void camerasAboutToBeChanged(QPrivateSignal);
    void camerasChanged(QPrivateSignal);

    void cameraAdded(const QnVirtualCameraResourcePtr& camera, QPrivateSignal);
    void cameraRemoved(const QnVirtualCameraResourcePtr& camera, QPrivateSignal);

private:
    void setCameras(Type type, const QnVirtualCameraResourceSet& cameras);
    QnVirtualCameraResourceSet allCameras() const; //< Filtered.

    void addCamera(const QnVirtualCameraResourcePtr& camera);
    void removeCamera(const QnVirtualCameraResourcePtr& camera);

private:
    const QnResourcePool* m_resourcePool;
    const Filter m_filter;
    Type m_type = Type::multiple;
    QnVirtualCameraResourceSet m_cameras;
};

} // namespace nx::client::desktop
