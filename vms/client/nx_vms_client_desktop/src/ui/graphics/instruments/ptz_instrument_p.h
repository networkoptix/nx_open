// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <qt_graphics_items/graphics_path_item.h>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/ui/common/custom_cursors.h>
#include <ui/graphics/items/generic/framed_widget.h>
#include <ui/graphics/items/generic/gui_elements_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/splash_item.h>
#include <ui/graphics/items/generic/text_button_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <utils/common/scoped_painter_rollback.h>

#include "selection_item.h"

using nx::vms::client::core::Geometry;

// -------------------------------------------------------------------------- //
// PtzArrowItem
// -------------------------------------------------------------------------- //
class PtzArrowItem: public GraphicsPathItem {
    Q_OBJECT
    typedef GraphicsPathItem base_type;

public:
    PtzArrowItem(QGraphicsItem* parent = nullptr):
        base_type(parent)
    {
        setAcceptedMouseButtons(Qt::NoButton);

        setPen(QPen(nx::vms::client::core::colorTheme()->color("ptz.main", 192), 0.0));
        setBrush(nx::vms::client::core::colorTheme()->color("ptz.fill", 192));
    }

    const QSizeF &size() const {
        return m_size;
    }

    void setSize(const QSizeF &size) {
        if(qFuzzyEquals(size, m_size))
            return;

        m_size = size;

        invalidatePath();
    }

    void moveTo(const QPointF &origin, const QPointF &target) {
        setPos(target);
        setRotation(Geometry::atan2(origin - target) / M_PI * 180);
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override {
        ensurePath();

        base_type::paint(painter, option, widget);
    }

protected:
    void invalidatePath() {
        m_pathValid = false;
    }

    void ensurePath() {
        if(m_pathValid)
            return;

        QPainterPath path;
        path.moveTo(0, 0);
        path.lineTo(m_size.height(), m_size.width() / 2);
        path.lineTo(m_size.height(), -m_size.width() / 2);
        path.closeSubpath();

        setPath(path);

        QPen pen = this->pen();
        pen.setWidthF(qMax(m_size.width(), m_size.height()) / 16.0);
        setPen(pen);
    }

private:
    /** Arrow size. Height determines arrow length. */
    QSizeF m_size{32, 32};

    bool m_pathValid = false;
};


// -------------------------------------------------------------------------- //
// PtzImageButtonWidget
// -------------------------------------------------------------------------- //
class PtzImageButtonWidget: public QnTextButtonWidget {
    Q_OBJECT
    typedef QnTextButtonWidget base_type;

public:
    PtzImageButtonWidget(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = {}):
        base_type(parent, windowFlags)
    {
        setFrameShape(Qn::EllipticalFrame);
        setRelativeFrameWidth(0.2 / 16.0);

        setStateOpacity(Default, 0.4);
        setStateOpacity(Hovered, 0.7);
        setStateOpacity(Pressed, 1.0);

        QFont font = this->font();
        font.setPixelSize(50);
        setFont(font);

        setFrameColor(nx::vms::client::core::colorTheme()->color("ptz.main", 192));
        setWindowColor(nx::vms::client::core::colorTheme()->color("ptz.fill", 64));
    }

    QnMediaResourceWidget *target() const {
        return m_target.data();
    }

    void setTarget(QnMediaResourceWidget *target) {
        m_target = target;
    }

private:
    QPointer<QnMediaResourceWidget> m_target;
};

// -------------------------------------------------------------------------- //
// PtzManipulatorWidget
// -------------------------------------------------------------------------- //
class PtzManipulatorWidget: public GraphicsWidget {
    Q_OBJECT
    typedef GraphicsWidget base_type;

public:
    PtzManipulatorWidget(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = {}):
        base_type(parent, windowFlags),
        m_pen(QPen(nx::vms::client::core::colorTheme()->color("ptz.main", 192), 0.0)),
        m_brush(nx::vms::client::core::colorTheme()->color("ptz.fill", 64))
    {
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        QRectF rect = this->rect();
        qreal penWidth = qMin(rect.width(), rect.height()) / 16;
        QPointF center = rect.center();
        QPointF centralStep = QPointF(penWidth, penWidth);

        QPen pen = m_pen;
        pen.setWidthF(penWidth);

        QnScopedPainterPenRollback penRollback(painter, pen);
        QnScopedPainterBrushRollback brushRollback(painter, m_brush);
        painter->drawEllipse(rect);
        painter->drawEllipse(QRectF(center - centralStep, center + centralStep));
    }

private:
    const QPen m_pen;
    const QBrush m_brush;
};

// -----------------------------------------------------------------------------------------------
// PtzNewArrowItem
// -----------------------------------------------------------------------------------------------
class PtzNewArrowItem: public QGraphicsWidget
{
    Q_OBJECT
    using base_type = QGraphicsWidget;

public:
    PtzNewArrowItem(QGraphicsWidget* parent = nullptr): base_type(parent) {}
    virtual ~PtzNewArrowItem() override = default;

    // Sized direction.
    QPointF direction() const { return m_direction; }

    void setDirection(const QPointF& value)
    {
        static constexpr qreal kArrowLength = 5.0;
        static constexpr qreal kHalfArrowWidth = 5.5;
        static constexpr qreal kHalfShaftWidth = 2.5;
        static constexpr qreal kMaximumLength = 200.0;

        m_direction = value;
        m_length = qMin(qSqrt(QPointF::dotProduct(m_direction, m_direction)), kMaximumLength);
        m_angle = qRadiansToDegrees(qAtan2(m_direction.y(), m_direction.x()));

        const auto arrowX = qMax(kArrowLength, m_length - kArrowLength);

        m_arrowPath = QPainterPath();
        m_arrowPath.moveTo(0.0, kHalfShaftWidth);
        m_arrowPath.lineTo(arrowX, kHalfShaftWidth);
        m_arrowPath.lineTo(arrowX, kHalfArrowWidth);
        m_arrowPath.lineTo(arrowX + kArrowLength, 0.0);
        m_arrowPath.lineTo(arrowX, -kHalfArrowWidth);
        m_arrowPath.lineTo(arrowX, -kHalfShaftWidth);
        m_arrowPath.lineTo(0.0, -kHalfShaftWidth);
        m_arrowPath.closeSubpath();

        update();
    }

    virtual void paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/,
        QWidget* /*widget*/) override
    {
        if (m_direction.isNull())
            return;

        static const QColor kShadowColor(nx::vms::client::core::colorTheme()->color("dark5", 51));
        static const QBrush kDarkBrush(nx::vms::client::core::colorTheme()->color("dark5", 102));
        static const QBrush kMidlightBrush(nx::vms::client::core::colorTheme()->color("light1", 153));
        static const QBrush kLightBrush(nx::vms::client::core::colorTheme()->color("light1"));

        static constexpr qreal kBigRadius = 9.5;
        static constexpr qreal kSmallRadius = 2.5;
        static constexpr qreal kMinimumLength = 10.0;

        QnScopedPainterPenRollback penRollback(painter, Qt::NoPen);
        QnScopedPainterBrushRollback brushRollback(painter, kDarkBrush);
        QnScopedPainterAntialiasingRollback aaRollback(painter, true);

        painter->drawEllipse(QPointF(0, 0), kBigRadius, kBigRadius);

        if (m_length >= kMinimumLength)
        {
            QnScopedPainterTransformRollback transformRollback(painter);
            painter->rotate(m_angle);
            painter->setBrush(kMidlightBrush);
            painter->setPen({kShadowColor, 1.0, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin});
            painter->drawPath(m_arrowPath);
        }

        painter->setPen(Qt::NoPen);
        painter->setBrush(kLightBrush);
        painter->drawEllipse(QPointF(0, 0), kSmallRadius, kSmallRadius);
    }

private:
    QPointF m_direction;
    qreal m_length = 0.0;
    qreal m_angle = 0.0;
    QPainterPath m_arrowPath;
};

// -------------------------------------------------------------------------- //
// PtzElementsWidget
// -------------------------------------------------------------------------- //
class PtzElementsWidget: public QnGuiElementsWidget
{
    Q_OBJECT
    typedef QnGuiElementsWidget base_type;

public:
    PtzElementsWidget(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = {}):
        base_type(parent, windowFlags),
        m_arrowItem(new PtzArrowItem(this)),
        m_newArrowItem(new PtzNewArrowItem(this))
    {
        setAcceptedMouseButtons(Qt::NoButton);
        m_arrowItem->setOpacity(0.0);
        m_newArrowItem->setOpacity(0.0);
    }

    PtzArrowItem* arrowItem() const { return m_arrowItem; }
    PtzNewArrowItem* newArrowItem() const { return m_newArrowItem; }

private:
    PtzArrowItem* const m_arrowItem;
    PtzNewArrowItem* const m_newArrowItem;
};

// -------------------------------------------------------------------------- //
// PtzSplashItem
// -------------------------------------------------------------------------- //
class PtzSplashItem: public QnSplashItem
{
    Q_OBJECT
    typedef QnSplashItem base_type;

public:
    PtzSplashItem(QGraphicsItem *parent = nullptr):
        base_type(parent)
    {
        setColor(nx::vms::client::core::colorTheme()->color("ptz.main", 128));
    }
};


// -------------------------------------------------------------------------- //
// PtzSelectionItem
// -------------------------------------------------------------------------- //
class PtzSelectionItem: public FixedArSelectionItem
{
    Q_OBJECT
    typedef FixedArSelectionItem base_type;

public:
    PtzSelectionItem(QGraphicsItem *parent = nullptr):
        base_type(parent)
    {
        setPen(QPen(nx::vms::client::core::colorTheme()->color("ptz.main", 192), 0.0));
        setBrush(nx::vms::client::core::colorTheme()->color("ptz.fill", 64));
    }
};
