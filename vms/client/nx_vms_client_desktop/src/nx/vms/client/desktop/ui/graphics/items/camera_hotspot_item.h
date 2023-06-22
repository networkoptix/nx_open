// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>
#include <QtWidgets/QGraphicsObject>

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/common/resource/camera_hotspots_data.h>

class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class CameraHotspotItem: public QGraphicsObject
{
    using base_type = QGraphicsObject;

public:
    CameraHotspotItem(
        const nx::vms::common::CameraHotspotData& hotspotData,
        QnWorkbenchContext* context,
        QGraphicsItem* parent = nullptr);
    virtual ~CameraHotspotItem() override;

    virtual QRectF boundingRect() const override;

    nx::vms::common::CameraHotspotData hotspotData() const;

    virtual void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget = nullptr) override;

protected:
    virtual void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
