// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <core/resource/videowall_pc_data.h>
#include <utils/common/id.h>

#include <ui/graphics/items/resource/resource_widget.h>

class QnVideoWallItem;
class QGraphicsAnchorLayout;
class QGraphicsWidget;

/** Class for displaying single screen of the videowall resource on the scene. */
class QnVideowallScreenWidget: public QnResourceWidget
{
    Q_OBJECT
    using base_type = QnResourceWidget;

public:
    QnVideowallScreenWidget(
        nx::vms::client::desktop::SystemContext* systemContext,
        nx::vms::client::desktop::WindowContext* windowContext,
        QnWorkbenchItem* item,
        QGraphicsItem* parent = nullptr);

    virtual ~QnVideowallScreenWidget();

    /**
     * \returns                         Videowall associated with this widget.
     */
    QnVideoWallResourcePtr videowall() const;

protected:
    virtual Qn::RenderStatus paintChannelBackground(QPainter* painter, int channel,
        const QRectF& channelRect, const QRectF& paintRect) override;

    virtual int calculateButtonsVisibility() const override;
    virtual QString calculateTitleText() const override;
    virtual Qn::ResourceStatusOverlay calculateStatusOverlay() const override;

    virtual void at_itemDataChanged(int role) override;

    virtual int helpTopicAt(const QPointF& pos) const override;

private:
    void updateItems();
    void updateLayout(bool force = false);

    void at_videoWall_itemChanged(const QnVideoWallResourcePtr& videoWall,
        const QnVideoWallItem& item,
        const QnVideoWallItem& oldItem);
private:
    friend class QnVideowallItemWidget;

    QnVideoWallResourcePtr m_videowall;
    QList<QnVideoWallItem> m_items;

    QGraphicsWidget* m_mainOverlayWidget = nullptr;
    QGraphicsAnchorLayout* m_layout = nullptr;

    bool m_layoutUpdateRequired = true;
};
