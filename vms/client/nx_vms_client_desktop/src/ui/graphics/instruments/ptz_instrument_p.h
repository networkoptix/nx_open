// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <client/client_settings.h>
#include <qt_graphics_items/graphics_path_item.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/text_button_widget.h>
#include <ui/graphics/items/generic/gui_elements_widget.h>
#include <ui/graphics/items/generic/framed_widget.h>
#include <ui/graphics/items/generic/splash_item.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <utils/common/scoped_painter_rollback.h>

#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/ui/common/custom_cursors.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

#include "selection_item.h"

using nx::vms::client::core::Geometry;
using namespace nx::vms::client::desktop;

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

        setPen(QPen(colorTheme()->color("ptz.main", 192), 0.0));
        setBrush(colorTheme()->color("ptz.fill", 192));
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
        setRelativeFrameWidth(1.0 / 16.0);

        setStateOpacity(Default, 0.4);
        setStateOpacity(Hovered, 0.7);
        setStateOpacity(Pressed, 1.0);

        QFont font = this->font();
        font.setPixelSize(50);
        setFont(font);

        setFrameColor(colorTheme()->color("ptz.main", 192));
        setWindowColor(colorTheme()->color("ptz.fill", 64));
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
        m_pen(QPen(colorTheme()->color("ptz.main", 192), 0.0)),
        m_brush(colorTheme()->color("ptz.fill", 64))
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


// -------------------------------------------------------------------------- //
// PtzOverlayWidget
// -------------------------------------------------------------------------- //
class PtzOverlayWidget: public GraphicsWidget
{
    Q_OBJECT
    typedef GraphicsWidget base_type;

public:
    PtzOverlayWidget(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = {}):
        base_type(parent, windowFlags),
        m_markersMode(Qt::Horizontal | Qt::Vertical),
        m_pen(QPen(colorTheme()->color("ptz.main", 192), 0.0))
    {
        setAcceptedMouseButtons(Qt::NoButton);

        QPainterPath upRoundPath;
        upRoundPath.moveTo(0, 1);
        upRoundPath.lineTo(0, 0.5);
        upRoundPath.arcTo(0, 0, 1, 1, 180, -180);
        upRoundPath.lineTo(1, 1);
        upRoundPath.closeSubpath();

        QPainterPath downRoundPath;
        downRoundPath.moveTo(0, 0);
        downRoundPath.lineTo(0, 0.5);
        downRoundPath.arcTo(0, 0, 1, 1, 180, 180);
        downRoundPath.lineTo(1, 0);
        downRoundPath.closeSubpath();

        /* Note that construction order is important as it defines which items are on top. */
        m_manipulatorWidget = new PtzManipulatorWidget(this);

        m_zoomInButton = new PtzImageButtonWidget(this);
        m_zoomInButton->setIcon(qnSkin->icon("ptz/zoom_in.png"));
        m_zoomInButton->setToolTip(tr("Zoom In"));

        m_zoomOutButton = new PtzImageButtonWidget(this);
        m_zoomOutButton->setIcon(qnSkin->icon("ptz/zoom_out.png"));
        m_zoomOutButton->setToolTip(tr("Zoom Out"));

        m_focusInButton = new PtzImageButtonWidget(this);
        m_focusInButton->setIcon(qnSkin->icon("ptz/focus_in.png"));
        m_focusInButton->setToolTip(tr("Focus Far"));
        m_focusInButton->setFrameShape(Qn::CustomFrame);
        m_focusInButton->setCustomFramePath(upRoundPath);

        m_focusOutButton = new PtzImageButtonWidget(this);
        m_focusOutButton->setIcon(qnSkin->icon("ptz/focus_out.png"));
        m_focusOutButton->setToolTip(tr("Focus Near"));
        m_focusOutButton->setFrameShape(Qn::CustomFrame);
        m_focusOutButton->setCustomFramePath(downRoundPath);

        m_focusAutoButton = new PtzImageButtonWidget(this);
        m_focusAutoButton->setIcon(qnSkin->icon("ptz/focus_auto.png"));
        m_focusAutoButton->setToolTip(tr("Auto Focus"));
        m_focusAutoButton->setFrameShape(Qn::RectangularFrame);

        m_modeButton = new PtzImageButtonWidget(this);
        m_modeButton->setToolTip(tr("Change Dewarping Mode"));

        updateLayout();
        showCursor();
    }

    void forceUpdateLayout()
    {
        updateLayout();
    }

    void hideCursor()
    {
        manipulatorWidget()->setCursor(Qt::BlankCursor);
        zoomInButton()->setCursor(Qt::BlankCursor);
        zoomOutButton()->setCursor(Qt::BlankCursor);
        focusInButton()->setCursor(Qt::BlankCursor);
        focusOutButton()->setCursor(Qt::BlankCursor);
        focusAutoButton()->setCursor(Qt::BlankCursor);
    }

    void showCursor()
    {
        manipulatorWidget()->setCursor(nx::vms::client::desktop::CustomCursors::sizeAll);
        zoomInButton()->setCursor(Qt::ArrowCursor);
        zoomOutButton()->setCursor(Qt::ArrowCursor);
        focusInButton()->setCursor(Qt::ArrowCursor);
        focusOutButton()->setCursor(Qt::ArrowCursor);
        focusAutoButton()->setCursor(Qt::ArrowCursor);
    }

    PtzManipulatorWidget* manipulatorWidget() const
    {
        return m_manipulatorWidget;
    }

    PtzImageButtonWidget* zoomInButton() const
    {
        return m_zoomInButton;
    }

    PtzImageButtonWidget* zoomOutButton() const
    {
        return m_zoomOutButton;
    }

    PtzImageButtonWidget* focusInButton() const
    {
        return m_focusInButton;
    }

    PtzImageButtonWidget* focusOutButton() const
    {
        return m_focusOutButton;
    }

    PtzImageButtonWidget* focusAutoButton() const
    {
        return m_focusAutoButton;
    }

    PtzImageButtonWidget* modeButton() const
    {
        return m_modeButton;
    }

    Qt::Orientations markersMode() const
    {
        return m_markersMode;
    }

    void setMarkersMode(Qt::Orientations mode)
    {
        m_markersMode = mode;
    }

    virtual void setGeometry(const QRectF& rect) override
    {
        QSizeF oldSize = size();

        base_type::setGeometry(rect);

        if(!qFuzzyEquals(oldSize, size()))
            updateLayout();
    }

    virtual void paint(
        QPainter* painter,
        const QStyleOptionGraphicsItem* option,
        QWidget* widget) override
    {
        if (m_markersMode == 0)
        {
            base_type::paint(painter, option, widget);
            return;
        }

        QRectF rect = this->rect();

        QVector<QPointF> crosshairLines;

        QPointF center = rect.center();
        qreal unit = this->unit();

        if (m_markersMode & Qt::Horizontal) {
            qreal x = unit * 3.0;
            while(x < rect.width() / 2.0) {
                crosshairLines
                    << center + QPointF(-x, unit / 2.0) << center + QPointF(-x, -unit / 2.0);

                if(x < rect.width() / 2.0 - 3.0 * unit || !m_focusInButton->isVisible())
                    crosshairLines
                    << center + QPointF( x, unit / 2.0) << center + QPointF( x, -unit / 2.0);

                x += unit;
            }
        }

        if (m_markersMode & Qt::Vertical) {
            qreal y = unit * 3.0;
            while(y < rect.height() / 2.0) {
                crosshairLines
                    << center + QPointF(unit / 2.0,  y) << center + QPointF(-unit / 2.0,  y)
                    << center + QPointF(unit / 2.0, -y) << center + QPointF(-unit / 2.0, -y);
                y += unit;
            }
        }

        QnScopedPainterPenRollback penRollback(painter, m_pen);
        painter->drawLines(crosshairLines);
    }

private:
    qreal unit() const
    {
        QRectF rect = this->rect();
        return qMin(rect.width(), rect.height()) / 32;
    }

    void updateLayout()
    {
        /* We're doing manual layout of child items as this is an overlay widget and
         * we don't want layouts to clash with widget's size constraints. */

        qreal unit = this->unit();
        QRectF rect = this->rect();
        QPointF center = rect.center();
        QPointF left = (rect.topLeft() + rect.bottomLeft()) / 2.0;
        QPointF right = (rect.topRight() + rect.bottomRight()) / 2.0;
        QPointF xStep(unit, 0), yStep(0, unit);
        QSizeF size = Geometry::toSize(xStep + yStep);

        m_manipulatorWidget->setGeometry(QRectF(center - xStep - yStep, center + xStep + yStep));

        if (qnSettings->isPtzAimOverlayEnabled())
        {
            m_zoomInButton->setGeometry(QRectF(center - xStep * 3 - yStep * 2.5, 1.5 * size));
            m_zoomOutButton->setGeometry(QRectF(center + xStep * 1.5 - yStep * 2.5, 1.5 * size));
            m_modeButton->setGeometry(QRectF(left + xStep - yStep * 1.5, 3.0 * size));

            if (m_focusAutoButton->isVisible())
            {
                m_focusInButton->setGeometry(QRectF(right - xStep * 2.5 - yStep * 2.25, 1.5 * size));
                m_focusAutoButton->setGeometry(QRectF(right - xStep * 2.5 - yStep * 0.75, 1.5 * size));
                m_focusOutButton->setGeometry(QRectF(right - xStep * 2.5 + yStep * 0.75, 1.5 * size));
            }
            else
            {
                m_focusInButton->setGeometry(QRectF(right - xStep * 2.5 - yStep * 1.5, 1.5 * size));
                m_focusOutButton->setGeometry(QRectF(right - xStep * 2.5 + yStep * 0.0, 1.5 * size));
            }
        }
        else
        {
            m_zoomInButton->setGeometry(QRectF(left + xStep * 0.5 - yStep * 2.0, 1.5 * size));
            m_zoomOutButton->setGeometry(QRectF(left + xStep * 0.5 + yStep * 0.5, 1.5 * size));
            m_modeButton->setGeometry(QRectF(left + xStep * 2.5 - yStep * 1.5, 3.0 * size));

            if (m_focusAutoButton->isVisible())
            {
                m_focusInButton->setGeometry(QRectF(right - xStep * 2.0 - yStep * 2.25, 1.5 * size));
                m_focusAutoButton->setGeometry(QRectF(right - xStep * 2.0 - yStep * 0.75, 1.5 * size));
                m_focusOutButton->setGeometry(QRectF(right - xStep * 2.0 + yStep * 0.75, 1.5 * size));
            }
            else
            {
                m_focusInButton->setGeometry(QRectF(right - xStep * 2.0 - yStep * 1.5, 1.5 * size));
                m_focusOutButton->setGeometry(QRectF(right - xStep * 2.0 + yStep * 0.0, 1.5 * size));
            }
        }
    }

private:
    Qt::Orientations m_markersMode;
    PtzManipulatorWidget* m_manipulatorWidget = nullptr;
    PtzImageButtonWidget* m_zoomInButton = nullptr;
    PtzImageButtonWidget* m_zoomOutButton = nullptr;
    PtzImageButtonWidget* m_focusInButton = nullptr;
    PtzImageButtonWidget* m_focusOutButton = nullptr;
    PtzImageButtonWidget* m_focusAutoButton = nullptr;
    PtzImageButtonWidget* m_modeButton = nullptr;
    const QPen m_pen;
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

        static const QColor kShadowColor(colorTheme()->color("dark5", 51));
        static const QBrush kDarkBrush(colorTheme()->color("dark5", 102));
        static const QBrush kMidlightBrush(colorTheme()->color("light1", 153));
        static const QBrush kLightBrush(colorTheme()->color("light1"));

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
        setColor(colorTheme()->color("ptz.main", 128));
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
        setPen(QPen(colorTheme()->color("ptz.main", 192), 0.0));
        setBrush(colorTheme()->color("ptz.fill", 64));
    }
};
