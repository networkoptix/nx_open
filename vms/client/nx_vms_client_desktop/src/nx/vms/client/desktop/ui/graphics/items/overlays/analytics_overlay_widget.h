// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <qt_graphics_items/graphics_widget.h>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

#include "figure/types.h"

namespace nx::vms::client::desktop {

/** Overlay widget which draws different types of objects. */
class AnalyticsOverlayWidget: public GraphicsWidget
{
    Q_OBJECT

    using base_type = GraphicsWidget;

public:
    struct AreaInfo
    {
        nx::Uuid id;
        QString text;
        QString hoverText;
    };

    AnalyticsOverlayWidget(QGraphicsWidget* parent);
    virtual ~AnalyticsOverlayWidget() override;

    void addOrUpdateArea(
        const AreaInfo& info,
        const figure::FigurePtr& figure);
    void removeArea(const nx::Uuid& areaId);

    void setZoomRect(const QRectF& valueRect);

    virtual void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override;

    virtual bool event(QEvent* event) override;

signals:
    void highlightedAreaChanged(const nx::Uuid& areaId);
    void areaClicked(const nx::Uuid& areaId);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
