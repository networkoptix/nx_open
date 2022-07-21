// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/common/resource/camera_hotspots_data.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>

class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class CameraHotspotsOverlayWidget: public QnViewportBoundWidget
{
    using base_type = QnViewportBoundWidget;

public:
    CameraHotspotsOverlayWidget(QGraphicsItem* parent = nullptr);
    virtual ~CameraHotspotsOverlayWidget() override;

    void initHotspots(
        QnWorkbenchContext* context,
        const QnVirtualCameraResourcePtr& camera,
        const nx::vms::common::CameraHotspotDataList& hotspotsData);

protected:
    virtual void resizeEvent(QGraphicsSceneResizeEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
