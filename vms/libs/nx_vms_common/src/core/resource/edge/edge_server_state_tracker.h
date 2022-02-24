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
     * Checks existing camera resources in the resource pool for presence of EDGE device own
     * camera, no matter is it child of considered EDGE server or not, also gathers list of server
     * child devices. Starts further camera resources tracking.
     * @note This method is not called from constructor, since querying resource pool within
     *     setResourcePool() scope may cause deadlock. Initialization done on 'server added'
     *     resource pool notification instead.
     */
    void initializeCamerasTracking();

    /**
     * @returns True if camera resource binded with EDGE server (physically, as single device)
     *     is an server's child.
     */
    bool hasCoupledCamera() const;

    /**
     * @returns True if camera resource binded with EDGE server is server's one and only
     *     child. That means than EDGE server may be displayed in the reduced state i.e as
     *     single camera among other servers. Actual display state depends, for example,
     *     also on QnMediaServerResource::isRedundancy(), so canonical state should be treated
     *     only as possibility to be displayed in reduced state.
     */
    bool hasCanonicalState() const;

    /**
     * @returns Valid pointer to the camera resource binded with EDGE server if such exists
     *     among all cameras in the system. It shouldn't be necessarily server's child. Null
     *     pointer if no such found.
     */
    QnVirtualCameraResourcePtr coupledCamera() const;

signals:
    void hasCoupledCameraChanged();
    void hasCanonicalStateChanged();

private:
    void updateHasCoupledCamera();
    void updateHasCanonicalState();

    void onResourceAdded(const QnResourcePtr& resource);
    void onResourceRemoved(const QnResourcePtr& resource);
    void onCameraParentIdChanged(const QnResourcePtr& resource, const QnUuid& previousParentId);
    bool isCoupledCamera(const QnVirtualCameraResourcePtr& camera) const;

private:
    QnMediaServerResource* m_edgeServer;
    QnVirtualCameraResourcePtr m_coupledCamera;
    QnUuidSet m_childCamerasIds;

    bool m_camerasTrackingInitialized = false;

    bool m_hasCoupledCameraPreviousValue = false;
    bool m_hasCanonicalStatePreviousValue = false;
};

} // namespace edge
} // namespace nx::core::resource

