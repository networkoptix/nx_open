// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <recording/time_period.h>
#include <ui/common/help_topic_queryable.h>
#include <qt_graphics_items/graphics_widget.h>

class QnTimeSlider;

namespace nx::vms::client::desktop::workbench::timeline {

/**
 * Represents timeline Thumbnail Panel rectangle. Is responsible for listening events only,
 * while real thumbnails painting is peformed by QnTimeSlider for historical reasons.
 * Takes care of all coordinate and time position convertations between QnTimeSlider and
 * Thumbnail Panel coordinate systems.
 */
class ThumbnailPanel: public GraphicsWidget, public HelpTopicQueryable
{
    Q_OBJECT
    using base_type = GraphicsWidget;

public:
    ThumbnailPanel(QnTimeSlider* slider, QGraphicsItem *parent);
    virtual ~ThumbnailPanel();

    virtual void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget = nullptr) override;

    virtual int helpTopicAt(const QPointF&) const override;

    /**
     * @return Time period within the archive currently opened on scene, corresponding to
     *     full width of Thumbnail Panel.
     *     Differs from {QnTimeSlider::windowStart(), QnTimeSlider::windowEnd()} because
     *     QnTimeSlider and ThumbnailPanel have different widths.
     */
    QnTimePeriod fullTimePeriod() const;

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent* event) override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent* event) override;
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
    virtual void mousePressEvent(QGraphicsSceneMouseEvent* event) override;

private:
    qreal positionFromTime(std::chrono::milliseconds time) const;
    std::chrono::milliseconds timeFromPosition(const QPointF& position) const;

private:
    QnTimeSlider* m_slider;
};

} // namespace nx::vms::client::desktop::workbench::timeline
