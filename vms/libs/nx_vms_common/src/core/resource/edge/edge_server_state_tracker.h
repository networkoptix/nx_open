// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

namespace nx::core::resource {
namespace edge {

class EdgeServerStateTracker: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    EdgeServerStateTracker(QnMediaServerResource* edgeServer);

    /**
     * @return True if unique camera resource that represents camera from an EDGE device is a child
     *      of corresponding EDGE server. False if there are no such camera resource among the
     *      child cameras of the server or, the other way around, there are more than one child
     *      cameras representing the same EDGE camera with different IDs which may be result of
     *      auto discovery or systems merge operations.
     */
    bool hasUniqueCoupledChildCamera() const;

    /**
     * @return Pointer to the unique child camera resource that represents camera from an EDGE
     *     device if <tt>hasUniqueCoupledChildCamera()</tt> returns true. Null pointer otherwise.
     */
    QnVirtualCameraResourcePtr uniqueCoupledChildCamera() const;

    /**
     * @return True if unique camera resource that represents camera from an EDGE device is the one
     *     and only child of corresponding EDGE server. In such case EDGE server may be represented
     *     in the compact form in the Resource Tree, as camera within servers list. This method
     *     informs whether it's possible or not to display EDGE server in the reduced form. Actual
     *     display state depends on <tt>QnMediaServerResource::isRedundancy()</tt> method. Also see
     *     <tt>QnMediaServerResource::isHiddenEdgeServer()</tt>
     */
    bool hasCanonicalState() const;

signals:
    void hasCoupledCameraChanged();
    void hasCanonicalStateChanged();

private:

    /**
     * @note This method isn't called from the constructor, since querying resource pool within
     *     setResourcePool() scope may cause deadlock. Initialization done on 'server added'
     *     resource pool notification instead.
     */
    void initializeCamerasTracking();
    void trackCamera(const QnVirtualCameraResourcePtr& camera);

    void updateState();

    void onResourceAdded(const QnResourcePtr& resource);
    void onResourceRemoved(const QnResourcePtr& resource);
    void onCameraParentIdChanged(const QnResourcePtr& resource, const QnUuid& previousParentId);
    void onCoupledCameraNameChanged(const QnResourcePtr& resource);

private:
    QnMediaServerResource* m_edgeServer;
    QnUuidSet m_childCamerasIds;
    QnUuidSet m_coupledCamerasIds;

    bool m_camerasTrackingInitialized = false;

    bool m_hasUniqueCoupledChildCameraPreviousValue = false;
    bool m_hasCanonicalStatePreviousValue = false;
};

} // namespace edge
} // namespace nx::core::resource
