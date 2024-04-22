// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

    /** If filter condition is changed, managed camera set must be notified with this call. */
    void invalidateFilter();

    Filter filter() const;

signals:
    /**
     * These signals are sent when current selection type or actual camera set is changed.
     */
    void camerasAboutToBeChanged(QPrivateSignal);
    void camerasChanged(QPrivateSignal);

private:
    void setCameras(Type type, const QnVirtualCameraResourceSet& cameras);
    void handleResourcesAdded(const QnResourceList& resources);
    void handleResourcesRemoved(const QnResourceList& resources);
    QnVirtualCameraResourceSet filteredCameras() const;

    /** Function to filter out Cross-System cameras. */
    bool cameraBelongsToLocalResourcePool(const QnVirtualCameraResourcePtr& camera);

private:
    const QnResourcePool* m_resourcePool;
    const Filter m_filter;
    Type m_type = Type::multiple;
    QnVirtualCameraResourceSet m_cameras;
    QnVirtualCameraResourceSet m_notFilteredCameras;
};

} // namespace nx::vms::client::desktop
