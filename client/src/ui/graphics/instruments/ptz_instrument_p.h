#ifndef QN_PTZ_INSTRUMENT_P_H
#define QN_PTZ_INSTRUMENT_P_H

#include <utils/common/scoped_painter_rollback.h>

#include <ui/common/geometry.h>
#include <ui/graphics/items/standard/graphics_path_item.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/ui_elements_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/style/skin.h>


// -------------------------------------------------------------------------- //
// PtzArrowItem
// -------------------------------------------------------------------------- //
class PtzArrowItem: public GraphicsPathItem {
    Q_OBJECT
    typedef GraphicsPathItem base_type;

public:
    PtzArrowItem(QGraphicsItem *parent = NULL): 
        base_type(parent),
        m_size(QSizeF(32, 32)),
        m_pathValid(false)
    {
        setAcceptedMouseButtons(0);

        //setPen(QPen(ptzArrowBorderColor, 0.0));
        //setBrush(ptzArrowBaseColor);
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
        setStateOpacity(HOVERED, 0.7);
        setStateOpacity(PRESSED, 1.0);

        //setFrameColor(ptzItemBorderColor);
        //setWindowColor(ptzItemBaseColor);
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
    PtzManipulatorWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0): 
        base_type(parent, windowFlags) 
    {}

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        QRectF rect = this->rect();
        qreal penWidth = qMin(rect.width(), rect.height()) / 16;
        QPointF center = rect.center();
        QPointF centralStep = QPointF(penWidth, penWidth);

        //QN_SCOPED_PAINTER_PEN_ROLLBACK(painter, QPen(ptzItemBorderColor, penWidth));
        //QN_SCOPED_PAINTER_BRUSH_ROLLBACK(painter, ptzItemBaseColor);
        painter->drawEllipse(rect);
        painter->drawEllipse(QRectF(center - centralStep, center + centralStep));
    }
};


// -------------------------------------------------------------------------- //
// PtzOverlayWidget
// -------------------------------------------------------------------------- //
class PtzOverlayWidget: public GraphicsWidget {
    Q_OBJECT
    typedef GraphicsWidget base_type;

public:
    PtzOverlayWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0): 
        base_type(parent, windowFlags),
        m_markersVisible(true)
    {
        setAcceptedMouseButtons(0);

        /* Note that construction order is important as it defines which items are on top. */
        m_manipulatorWidget = new PtzManipulatorWidget(this);

        m_zoomInButton = new PtzImageButtonWidget(this);
        m_zoomInButton->setIcon(qnSkin->icon("item/ptz_zoom_in.png"));

        m_zoomOutButton = new PtzImageButtonWidget(this);
        m_zoomOutButton->setIcon(qnSkin->icon("item/ptz_zoom_out.png"));

        m_modeButton = new PtzImageButtonWidget(this);
        m_modeButton->setToolTip(tr("Dewarping panoramic mode"));

        updateLayout();
        showCursor();
    }

    void hideCursor() {
        manipulatorWidget()->setCursor(Qt::BlankCursor);
        zoomInButton()->setCursor(Qt::BlankCursor);
        zoomOutButton()->setCursor(Qt::BlankCursor);
    }

    void showCursor() {
        manipulatorWidget()->setCursor(Qt::SizeAllCursor);
        zoomInButton()->setCursor(Qt::ArrowCursor);
        zoomOutButton()->setCursor(Qt::ArrowCursor);
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

    PtzImageButtonWidget *modeButton() const {
        return m_modeButton;
    }

    bool isMarkersVisible() const {
        return m_markersVisible;
    }

    void setMarkersVisible(bool markersVisible) {
        m_markersVisible = markersVisible;
    }

    virtual void setGeometry(const QRectF &rect) override {
        QSizeF oldSize = size();

        base_type::setGeometry(rect);

        if(!qFuzzyEquals(oldSize, size()))
            updateLayout();
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
        if(!m_markersVisible) {
            base_type::paint(painter, option, widget);
            return;
        }

        QRectF rect = this->rect();

        QVector<QPointF> crosshairLines; // TODO: #Elric cache these?

        QPointF center = rect.center();
        qreal d0 = qMin(rect.width(), rect.height()) / 4.0;
        qreal d1 = d0 / 8.0;

        qreal dx = d1 * 3.0;
        while(dx < rect.width() / 2.0) {
            crosshairLines 
                << center + QPointF( dx, d1 / 2.0) << center + QPointF( dx, -d1 / 2.0)
                << center + QPointF(-dx, d1 / 2.0) << center + QPointF(-dx, -d1 / 2.0);
            dx += d1;
        }

        qreal dy = d1 * 3.0;
        while(dy < rect.height() / 2.0) {
            crosshairLines 
                << center + QPointF(d1 / 2.0,  dy) << center + QPointF(-d1 / 2.0,  dy)
                << center + QPointF(d1 / 2.0, -dy) << center + QPointF(-d1 / 2.0, -dy);
            dy += d1;
        }

        //QN_SCOPED_PAINTER_PEN_ROLLBACK(painter, QPen(ptzItemBorderColor, 0.0));
        painter->drawLines(crosshairLines);
    }

private:
    void updateLayout() {
        /* We're doing manual layout of child items as this is an overlay widget and
         * we don't want layouts to mess up widget's size constraints. */

        QRectF rect = this->rect();
        QPointF center = rect.center();
        qreal centralWidth = qMin(rect.width(), rect.height()) / 32;
        QPointF xStep(centralWidth, 0), yStep(0, centralWidth);

        m_manipulatorWidget->setGeometry(QRectF(center - xStep - yStep, center + xStep + yStep));
        m_zoomInButton->setGeometry(QRectF(center - xStep * 3 - yStep * 2.5, 1.5 * QnGeometry::toSize(xStep + yStep)));
        m_zoomOutButton->setGeometry(QRectF(center + xStep * 1.5 - yStep * 2.5, 1.5 * QnGeometry::toSize(xStep + yStep)));
        m_modeButton->setGeometry(QRectF((rect.topRight() + rect.bottomRight()) / 2.0 - xStep * 4.0 - yStep * 1.5, 3.0 * QnGeometry::toSize(xStep + yStep)));
    }

private:
    bool m_markersVisible;
    PtzManipulatorWidget *m_manipulatorWidget;
    PtzImageButtonWidget *m_zoomInButton;
    PtzImageButtonWidget *m_zoomOutButton;
    PtzImageButtonWidget *m_modeButton;
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



#endif // QN_PTZ_INSTRUMENT_P_H
