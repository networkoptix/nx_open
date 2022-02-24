// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QPoint>
#include <QtCore/QRect>
#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

#include <ui/utils/widget_opacity_mixin.h>

class QPainterPath;

namespace nx::vms::client::desktop {

/**
 * Rectangular tooltip with rounded corners and trapeziform tail at one of four borders.
 */
class BaseTooltip: public WidgetOpacityMixin<QWidget>
{
    Q_OBJECT

    using base_type = WidgetOpacityMixin<QWidget>;

public:

    /** Stores parameters of tail trapezoid */
    struct TailGeometry
    {
        /**
         * Distance between tail tip and tooltip rectangle edge. If tailBorder() is Qt::LeftEdge
         * or Qt::RightEdge, this is not literally height, but horizontal extent instead.
         */
        int height;

        /** Width of tail at tooltip rectangle edge. */
        int greaterWidth;

        /** Width of tail at tip. */
        int lesserWidth;
    };

    /**
     * @param tailBorder Direction of tooltip tail.
     * @param filterEvents If set to false, mouse and paint events will not be appropriated by
     *     BaseTooltip solely for its own.
     */
    BaseTooltip(Qt::Edge tailBorder, bool filterEvents = true, QWidget* parent = nullptr);

    /**
     * Move tooltip to make tail tip pointing to pos.
     * @param pos New tail tip position in parent widget coordinate system.
     * @param longitudinalBounds If non-null QRect specified, body of the tooltip will always lay
     *     between left() and right() of the rectangle horizontally if tailBorder() is TopEdge
     *     or BottomEdge; or between top() and bottom() vertically if tailBorder() is LeftEdge
     *     or RightEdge. Tail will be moved away from side center if needed. Specified in
     *     parent widget coordinate system.
     */
    void pointTo(const QPoint& pos, const QRect& longitudinalBounds = QRect());

    /**
     * @return Tail tip position relative to parent widget or {0, 0} if pointTo() was never called.
     */
    QPoint pointedTo() const;

    /**
     * @return Direction of tooltip tail.
     */
    Qt::Edge tailBorder() const;

    /** Avoid receiving input events. */
    void setTransparentForInput(bool enabled);

    /** @return Parent widget widget which serves as a basis for tooltip coordinates. */
    QWidget* tooltipParent() const { return m_parentWidget; }

signals:
    void clicked();
    void opacityChanged();

protected:
    virtual void setTailBorder(Qt::Edge tailBorder);

    TailGeometry tailGeometry() const;
    void setTailGeometry(const TailGeometry& geometry);

    /**
     * @return Bounding rect of tooltip balloon excluding tail.
     */
    virtual QRect bodyRect() const;

    void setRoundingRadius(int radius);

    virtual void paintEvent(QPaintEvent* event) override;
    virtual void colorizeBorderShape(const QPainterPath& borderShape);

private:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void moveToPointedPos();
    bool pointsVertically() const;
    int tailEdgeLength() const;
    int tailOffsetFromCenter() const;
    int tailOffset() const;
    int notOrientedTailOffset() const;
    void addTailedEdge(QPainterPath* path) const;

private:
    Qt::Edge m_tailBorder = Qt::TopEdge;
    int m_roundingRadius = 0;
    TailGeometry m_tailGeometry = {0, 0, 0};
    int m_transparentHeaderSize = 0;
    QRect m_tooltipRect;

    QPoint m_pointsTo;
    QRect m_longitudinalBounds;
    bool m_movingProcess = false;
    bool m_clicked = false;
    QPointer<QWidget> m_parentWidget;
};

} // namespace nx::vms::client::desktop
