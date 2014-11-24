#ifndef QN_PTZ_INSTRUMENT_P_H
#define QN_PTZ_INSTRUMENT_P_H

#include <utils/common/scoped_painter_rollback.h>

#include <ui/common/geometry.h>
#include <ui/customization/customized.h>
#include <ui/graphics/items/standard/graphics_path_item.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/text_button_widget.h>
#include <ui/graphics/items/generic/ui_elements_widget.h>
#include <ui/graphics/items/generic/framed_widget.h>
#include <ui/graphics/items/generic/splash_item.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/style/skin.h>

#include "selection_item.h"


// -------------------------------------------------------------------------- //
// PtzArrowItem
// -------------------------------------------------------------------------- //
class PtzArrowItem: public Customized<GraphicsPathItem> {
    Q_OBJECT
    typedef Customized<GraphicsPathItem> base_type;

public:
    PtzArrowItem(QGraphicsItem *parent = NULL): 
        base_type(parent),
        m_size(QSizeF(32, 32)),
        m_pathValid(false)
    {
        setAcceptedMouseButtons(0);

        setPen(QPen(QColor(128, 196, 255, 192), 0.0));
        setBrush(QColor(128, 196, 255, 192).lighter(120));
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
        setRotation(QnGeometry::atan2(origin - target) / M_PI * 180);
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = NULL) override {
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
    QSizeF m_size;

    bool m_pathValid;
};


// -------------------------------------------------------------------------- //
// PtzImageButtonWidget
// -------------------------------------------------------------------------- //
class PtzImageButtonWidget: public QnTextButtonWidget {
    Q_OBJECT
    typedef QnTextButtonWidget base_type;

public:
    PtzImageButtonWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags)
    {
        setFrameShape(Qn::EllipticalFrame);
        setRelativeFrameWidth(1.0 / 16.0);
        
        setStateOpacity(0, 0.4);
        setStateOpacity(Hovered, 0.7);
        setStateOpacity(Pressed, 1.0);

        QFont font = this->font();
        font.setPixelSize(50);
        setFont(font);

        setFrameColor(QColor(128, 196, 255, 192));
        setWindowColor(QColor(128, 196, 255, 64).lighter(120));
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
    Q_PROPERTY(QPen pen READ pen WRITE setPen)
    Q_PROPERTY(QBrush brush READ brush WRITE setBrush)
    typedef GraphicsWidget base_type;

public:
    PtzManipulatorWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0): 
        base_type(parent, windowFlags) 
    {
        setPen(QPen(QColor(128, 196, 255, 192), 0.0));
        setBrush(QColor(128, 196, 255, 64).lighter(120));
    }

    const QBrush &brush() const { return m_brush; }
    void setBrush(const QBrush &brush) { m_brush = brush; }

    const QPen &pen() const { return m_pen; }
    void setPen(const QPen &pen) { m_pen = pen; }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        QRectF rect = this->rect();
        qreal penWidth = qMin(rect.width(), rect.height()) / 16;
        QPointF center = rect.center();
        QPointF centralStep = QPointF(penWidth, penWidth);

        QPen pen = m_pen;
        pen.setWidthF(penWidth);

        QN_SCOPED_PAINTER_PEN_ROLLBACK(painter, pen);
        QN_SCOPED_PAINTER_BRUSH_ROLLBACK(painter, m_brush);
        painter->drawEllipse(rect);
        painter->drawEllipse(QRectF(center - centralStep, center + centralStep));
    }

private:
    QPen m_pen;
    QBrush m_brush;
};


// -------------------------------------------------------------------------- //
// PtzOverlayWidget
// -------------------------------------------------------------------------- //
class PtzOverlayWidget: public GraphicsWidget {
    Q_OBJECT
    Q_PROPERTY(QPen pen READ pen WRITE setPen)
    typedef GraphicsWidget base_type;

public:
    PtzOverlayWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0): 
        base_type(parent, windowFlags),
        m_markersMode(Qt::Horizontal | Qt::Vertical)
    {
        setAcceptedMouseButtons(0);

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
        m_zoomInButton->setIcon(qnSkin->icon("item/ptz_zoom_in.png"));
        m_zoomInButton->setToolTip(tr("Zoom In"));

        m_zoomOutButton = new PtzImageButtonWidget(this);
        m_zoomOutButton->setIcon(qnSkin->icon("item/ptz_zoom_out.png"));
        m_zoomOutButton->setToolTip(tr("Zoom Out"));

        m_focusInButton = new PtzImageButtonWidget(this);
        m_focusInButton->setIcon(qnSkin->icon("item/ptz_focus_in.png"));
        m_focusInButton->setToolTip(tr("Focus Far"));
        m_focusInButton->setFrameShape(Qn::CustomFrame);
        m_focusInButton->setCustomFramePath(upRoundPath);

        m_focusOutButton = new PtzImageButtonWidget(this);
        m_focusOutButton->setIcon(qnSkin->icon("item/ptz_focus_out.png"));
        m_focusOutButton->setToolTip(tr("Focus Near"));
        m_focusOutButton->setFrameShape(Qn::CustomFrame);
        m_focusOutButton->setCustomFramePath(downRoundPath);

        m_focusAutoButton = new PtzImageButtonWidget(this);
        m_focusAutoButton->setIcon(qnSkin->icon("item/ptz_focus_auto.png"));
        m_focusAutoButton->setToolTip(tr("Auto Focus"));
        m_focusAutoButton->setFrameShape(Qn::RectangularFrame);

        m_modeButton = new PtzImageButtonWidget(this);
        m_modeButton->setToolTip(tr("Change Dewarping Mode"));

        connect(m_focusAutoButton, &QGraphicsObject::visibleChanged, this, &PtzOverlayWidget::updateLayout);

        updateLayout();
        showCursor();

        setPen(QPen(QColor(128, 196, 255, 192), 0.0));
    }

    const QPen &pen() const { return m_pen; }
    void setPen(const QPen &pen) { m_pen = pen; }

    void hideCursor() {
        manipulatorWidget()->setCursor(Qt::BlankCursor);
        zoomInButton()->setCursor(Qt::BlankCursor);
        zoomOutButton()->setCursor(Qt::BlankCursor);
        focusInButton()->setCursor(Qt::BlankCursor);
        focusOutButton()->setCursor(Qt::BlankCursor);
        focusAutoButton()->setCursor(Qt::BlankCursor);
    }

    void showCursor() {
        manipulatorWidget()->setCursor(Qt::SizeAllCursor);
        zoomInButton()->setCursor(Qt::ArrowCursor);
        zoomOutButton()->setCursor(Qt::ArrowCursor);
        focusInButton()->setCursor(Qt::ArrowCursor);
        focusOutButton()->setCursor(Qt::ArrowCursor);
        focusAutoButton()->setCursor(Qt::ArrowCursor);
    }

    PtzManipulatorWidget *manipulatorWidget() const {
        return m_manipulatorWidget;
    }

    PtzImageButtonWidget *zoomInButton() const {
        return m_zoomInButton;
    }

    PtzImageButtonWidget *zoomOutButton() const {
        return m_zoomOutButton;
    }

    PtzImageButtonWidget *focusInButton() const {
        return m_focusInButton;
    }

    PtzImageButtonWidget *focusOutButton() const {
        return m_focusOutButton;
    }

    PtzImageButtonWidget *focusAutoButton() const {
        return m_focusAutoButton;
    }

    PtzImageButtonWidget *modeButton() const {
        return m_modeButton;
    }

    Qt::Orientations markersMode() const {
        return m_markersMode;
    }

    void setMarkersMode(Qt::Orientations mode) {
        m_markersMode = mode;
    }

    virtual void setGeometry(const QRectF &rect) override {
        QSizeF oldSize = size();

        base_type::setGeometry(rect);

        if(!qFuzzyEquals(oldSize, size()))
            updateLayout();
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
        if(m_markersMode == 0) {
            base_type::paint(painter, option, widget);
            return;
        }

        QRectF rect = this->rect();

        QVector<QPointF> crosshairLines; // TODO: #Elric cache these?

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

        QN_SCOPED_PAINTER_PEN_ROLLBACK(painter, m_pen);
        painter->drawLines(crosshairLines);
    }

private:
    qreal unit() const {
        QRectF rect = this->rect();
        return qMin(rect.width(), rect.height()) / 32;
    }

    void updateLayout() {
        /* We're doing manual layout of child items as this is an overlay widget and
         * we don't want layouts to mess up widget's size constraints. */

        qreal unit = this->unit();
        QRectF rect = this->rect();
        QPointF center = rect.center();
        QPointF left = (rect.topLeft() + rect.bottomLeft()) / 2.0;
        QPointF right = (rect.topRight() + rect.bottomRight()) / 2.0;
        QPointF xStep(unit, 0), yStep(0, unit);
        QSizeF size = QnGeometry::toSize(xStep + yStep);

        m_manipulatorWidget->setGeometry(QRectF(center - xStep - yStep, center + xStep + yStep));
        m_modeButton->setGeometry(QRectF(right - xStep * 4.0 - yStep * 1.5, 3.0 * size));

        m_zoomInButton->setGeometry(QRectF(center - xStep * 3 - yStep * 2.5, 1.5 * size));
        m_zoomOutButton->setGeometry(QRectF(center + xStep * 1.5 - yStep * 2.5, 1.5 * size));
        
        if(m_focusAutoButton->isVisible()) {
            m_focusInButton->setGeometry(QRectF(right - xStep * 2.5 - yStep * 2.25, 1.5 * size));
            m_focusAutoButton->setGeometry(QRectF(right - xStep * 2.5 - yStep * 0.75, 1.5 * size));
            m_focusOutButton->setGeometry(QRectF(right - xStep * 2.5 + yStep * 0.75, 1.5 * size));
        } else {
            m_focusInButton->setGeometry(QRectF(right - xStep * 2.5 - yStep * 1.5, 1.5 * size));
            m_focusOutButton->setGeometry(QRectF(right - xStep * 2.5 + yStep * 0.0, 1.5 * size));
        }
    }

private:
    Qt::Orientations m_markersMode;
    PtzManipulatorWidget *m_manipulatorWidget;
    PtzImageButtonWidget *m_zoomInButton;
    PtzImageButtonWidget *m_zoomOutButton;
    PtzImageButtonWidget *m_focusInButton;
    PtzImageButtonWidget *m_focusOutButton;
    PtzImageButtonWidget *m_focusAutoButton;
    PtzImageButtonWidget *m_modeButton;
    QPen m_pen;
};


// -------------------------------------------------------------------------- //
// PtzElementsWidget
// -------------------------------------------------------------------------- //
class PtzElementsWidget: public QnUiElementsWidget {
    Q_OBJECT
    typedef QnUiElementsWidget base_type;

public:
    PtzElementsWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags)
    {
        setAcceptedMouseButtons(0);

        m_arrowItem = new PtzArrowItem(this);
        m_arrowItem->setOpacity(0.0);
    }

    PtzArrowItem *arrowItem() const {
        return m_arrowItem;
    }

private:
    PtzArrowItem *m_arrowItem;
};


// -------------------------------------------------------------------------- //
// PtzSplashItem
// -------------------------------------------------------------------------- //
class PtzSplashItem: public Customized<QnSplashItem> {
    Q_OBJECT
    typedef Customized<QnSplashItem> base_type;

public:
    PtzSplashItem(QGraphicsItem *parent = NULL): 
        base_type(parent) 
    {
        setColor(QColor(128, 196, 255, 128));
    }
};


// -------------------------------------------------------------------------- //
// PtzSelectionItem
// -------------------------------------------------------------------------- //
class PtzSelectionItem: public Customized<FixedArSelectionItem> {
    Q_OBJECT
    typedef Customized<FixedArSelectionItem> base_type;

public:
    PtzSelectionItem(QGraphicsItem *parent = NULL):
        base_type(parent)
    {
        setPen(QPen(QColor(128, 196, 255, 192), 0.0));
        setBrush(QColor(128, 196, 255, 64).lighter(120));
    }
};


#endif // QN_PTZ_INSTRUMENT_P_H
