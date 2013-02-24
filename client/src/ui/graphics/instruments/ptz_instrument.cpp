#include "ptz_instrument.h"

#include <cmath>
#include <cassert>
#include <limits>

#include <QtCore/QVariant>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QApplication>

#include <utils/common/checked_cast.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/fuzzy.h>
#include <utils/math/math.h>
#include <utils/math/space_mapper.h>
#include <utils/math/color_transformations.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <api/media_server_connection.h>

#include <utils/math/coordinate_transformations.h>
#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/animation/animation_event.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_ptz_controller.h>
#include <ui/workbench/watchers/workbench_ptz_camera_watcher.h>
#include <ui/workbench/watchers/workbench_ptz_mapper_watcher.h>

#include "selection_item.h"
#include "utils/settings.h"

//#define QN_PTZ_INSTRUMENT_DEBUG
#ifdef QN_PTZ_INSTRUMENT_DEBUG
#   define TRACE(...) qDebug() << __VA_ARGS__;
#else
#   define TRACE(...)
#endif

namespace {
    const QColor ptzColor = qnGlobals->ptzColor();

    const QColor ptzArrowBorderColor = toTransparent(ptzColor, 0.75);
    const QColor ptzArrowBaseColor = toTransparent(ptzColor.lighter(120), 0.75);

    const QColor ptzItemBorderColor = toTransparent(ptzColor, 0.75);
    const QColor ptzItemBaseColor = toTransparent(ptzColor.lighter(120), 0.25);

    const qreal instantSpeedUpdateThreshold = 0.1;
    const qreal speedUpdateThreshold = 0.001;
    const int speedUpdateIntervalMSec = 500;
}


// -------------------------------------------------------------------------- //
// PtzSelectionItem
// -------------------------------------------------------------------------- //
class PtzSelectionItem: public SelectionItem {
    typedef SelectionItem base_type;

public:
    PtzSelectionItem(QGraphicsItem *parent = NULL): 
        base_type(parent),
        m_elementSize(0.0)
    {
        setPen(ptzItemBorderColor);
        setBrush(ptzItemBaseColor);
    }

    virtual QRectF boundingRect() const override {
        return QnGeometry::dilated(base_type::boundingRect(), m_elementSize / 2.0);
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        base_type::paint(painter, option, widget);

        QRectF rect = base_type::rect();
        QPointF center = rect.center();
        if(rect.isEmpty())
            return;

        QPainterPath path;
        qreal elementHalfSize = qMin(m_elementSize, qMin(rect.width(), rect.height()) / 2.0) / 2.0;
        path.addEllipse(center, elementHalfSize, elementHalfSize);

        foreach(const QPointF &sidePoint, m_sidePoints) {
            QPointF v = sidePoint - center;
            qreal l = QnGeometry::length(v);
            qreal a = -QnGeometry::atan2(v) / M_PI * 180;
            qreal da = 60.0;

            QPointF c = sidePoint - v / l * (elementHalfSize * 1.5);
            QPointF r = QPointF(elementHalfSize, elementHalfSize) * 1.5;
            QRectF rect(c - r, c + r);

            path.arcMoveTo(rect, a - da);
            path.arcTo(rect, a - da, 2 * da);
        }

        QnScopedPainterPenRollback penRollback(painter, QPen(pen().color().lighter(120), elementHalfSize / 2.0));
        QnScopedPainterBrushRollback brushRollback(painter, Qt::NoBrush);
        painter->drawPath(path);
    }

    qreal elementSize() const {
        return m_elementSize;
    }

    void setElementSize(qreal elementSize) {
        m_elementSize = elementSize;
    }

    const QVector<QPointF> &sidePoints() const {
        return m_sidePoints;
    }

    void setSidePoints(const QVector<QPointF> &sidePoints) {
        m_sidePoints = sidePoints;
    }

private:
    qreal m_elementSize;
    QVector<QPointF> m_sidePoints;
};



// -------------------------------------------------------------------------- //
// PtzSplashItem
// -------------------------------------------------------------------------- //
class PtzSplashItem: public QGraphicsObject {
    typedef QGraphicsObject base_type;

public:
    enum SplashType {
        Circular,
        Rectangular,
        Invalid = -1
    };

    PtzSplashItem(QGraphicsItem *parent = NULL):
        base_type(parent),
        m_splashType(Invalid)
    {
        setAcceptedMouseButtons(0);

        QGradient gradients[5];

        /* Sector numbering for rectangular splash:
         *       1
         *      0 2
         *       3                                                              */
        gradients[0] = QLinearGradient(1.0, 0.0, 0.0, 0.0);
        gradients[1] = QLinearGradient(0.0, 1.0, 0.0, 0.0);
        gradients[2] = QLinearGradient(0.0, 0.0, 1.0, 0.0);
        gradients[3] = QLinearGradient(0.0, 0.0, 0.0, 1.0);
        gradients[4] = QRadialGradient(0.5, 0.5, 0.5);

        for(int i = 0; i < 5; i++) {
            gradients[i].setCoordinateMode(QGradient::ObjectBoundingMode);
            gradients[i].setColorAt(0.8, toTransparent(ptzColor));
            gradients[i].setColorAt(0.9, toTransparent(ptzColor, 0.5));
            gradients[i].setColorAt(1.0, toTransparent(ptzColor));
            m_brushes[i] = QBrush(gradients[i]);
        }
    }

    SplashType splashType() const {
        return m_splashType;
    }
    
    void setSplashType(SplashType splashType) {
        assert(splashType == Circular || splashType == Rectangular || splashType == Invalid);

        m_splashType = splashType;
    }

    virtual QRectF boundingRect() const override {
        return rect();
    }

    const QRectF &rect() const {
        return m_rect;
    }

    void setRect(const QRectF &rect) {
        if(qFuzzyCompare(m_rect, rect))
            return;

        prepareGeometryChange();

        m_rect = rect;
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override {
        if(m_splashType == Circular) {
            painter->fillRect(m_rect, m_brushes[4]);
        } else if(splashType() == Rectangular) {
            QPointF points[5] = {
                m_rect.bottomLeft(),
                m_rect.topLeft(),
                m_rect.topRight(),
                m_rect.bottomRight(),
                m_rect.bottomLeft()
            };
            
            qreal d = qMin(m_rect.width(), m_rect.height()) / 2;
            QPointF centers[5] = {
                m_rect.bottomLeft()     + QPointF( d, -d),
                m_rect.topLeft()        + QPointF( d,  d),
                m_rect.topRight()       + QPointF(-d,  d),
                m_rect.bottomRight()    + QPointF(-d, -d),
                m_rect.bottomLeft()     + QPointF( d, -d)
            };

            for(int i = 0; i < 4; i++) {
                QPainterPath path;

                path = QPainterPath();
                path.moveTo(centers[i]);
                path.lineTo(points[i]);
                path.lineTo(points[i + 1]);
                path.lineTo(centers[i + 1]);
                path.closeSubpath();
                painter->fillPath(path, m_brushes[i]);
            }
        }
    }
        
private:
    /** Splash type. */
    SplashType m_splashType;

    /** Brushes that are used for painting. 0-3 for rectangular splash, 4 for circular. */
    QBrush m_brushes[5];

    /** Bounding rectangle of the splash. */
    QRectF m_rect;
};


// -------------------------------------------------------------------------- //
// PtzArrowItem
// -------------------------------------------------------------------------- //
class PtzArrowItem: public GraphicsPathItem {
    typedef GraphicsPathItem base_type;

public:
    PtzArrowItem(QGraphicsItem *parent = NULL): 
        base_type(parent),
        m_size(QSizeF(32, 32)),
        m_pathValid(false)
    {
        setAcceptedMouseButtons(0);

        setPen(ptzArrowBorderColor);
        setBrush(ptzArrowBaseColor);
    }

    const QSizeF &size() const {
        return m_size;
    }

    void setSize(const QSizeF &size) {
        if(qFuzzyCompare(size, m_size))
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
// PtzPointerItem
// -------------------------------------------------------------------------- //
class PtzPointerItem: public Animated<GraphicsPathItem>, public AnimationTimerListener {
    typedef Animated<GraphicsPathItem> base_type;

public:
    PtzPointerItem(QGraphicsItem *parent = NULL): 
        base_type(parent),
        m_size(QSizeF(0, 0)),
        m_pathValid(false)
    {
        registerAnimation(this);
        startListening();

        setAcceptedMouseButtons(0);

        setSize(QSizeF(32, 32));
        setBrush(Qt::NoBrush);
        setPen(ptzItemBorderColor);
    }

    const QSizeF &size() const {
        return m_size;
    }

    void setSize(const QSizeF &size) {
        if(qFuzzyCompare(size, m_size))
            return;

        m_size = size;

        setTransformOriginPoint(QnGeometry::toPoint(m_size / 2));
        invalidatePath();
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = NULL) override {
        ensurePath();

        base_type::paint(painter, option, widget);
    }

protected:
    virtual void tick(int deltaMSecs) override {
        setRotation(rotation() + deltaMSecs / 1000.0 * 540.0);
    }

    void updatePen() {
        setPen(QPen(ptzItemBorderColor, qMin(m_size.height(), m_size.width()) / 3));
    }

    void invalidatePath() {
        m_pathValid = false;
    }

    void ensurePath() {
        if(m_pathValid)
            return;

        QRectF rect(QPointF(0, 0), m_size);

        QPainterPath path;
        path.arcMoveTo(rect, 0);
        path.arcTo(rect, 0, 90);
        path.arcMoveTo(rect, 180);
        path.arcTo(rect, 180, 90);
        setPath(path);
        
        QPen pen = this->pen();
        pen.setWidthF(qMax(m_size.width(), m_size.height()) / 4.0);
        setPen(pen);
    }

private:
    /** Pointer size. */
    QSizeF m_size;

    bool m_pathValid;
};


// -------------------------------------------------------------------------- //
// PtzZoomButtonWidget
// -------------------------------------------------------------------------- //
class PtzZoomButtonWidget: public QnImageButtonWidget {
    typedef QnImageButtonWidget base_type;

public:
    PtzZoomButtonWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags)
    {}

    QnMediaResourceWidget *target() const {
        return m_target.data();
    }

    void setTarget(QnMediaResourceWidget *target) {
        m_target = target;
    }

protected:
    virtual void paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QGLWidget *widget) {
        qreal opacity = painter->opacity();
        painter->setOpacity(opacity * (stateOpacity(startState) * (1.0 - progress) + stateOpacity(endState) * progress));

        bool isPressed = (startState & PRESSED) || (endState & PRESSED);
        {
            QnScopedPainterPenRollback penRollback(painter, QPen(isPressed ? ptzArrowBorderColor : ptzItemBorderColor, qMax(size().height(), size().width()) / 16.0));
            QnScopedPainterBrushRollback brushRollback(painter, isPressed ? ptzArrowBaseColor : ptzItemBaseColor);
            painter->drawEllipse(rect());
        }

        base_type::paint(painter, startState, endState, progress, widget);

        painter->setOpacity(opacity);
    }

    qreal stateOpacity(StateFlags stateFlags) {
        return (stateFlags & HOVERED) ? 1.0 : 0.5;
    }

private:
    QWeakPointer<QnMediaResourceWidget> m_target;
};


// -------------------------------------------------------------------------- //
// PtzManipulatorWidget
// -------------------------------------------------------------------------- //
class PtzManipulatorWidget: public QGraphicsWidget {
    typedef QGraphicsWidget base_type;

public:
    PtzManipulatorWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0): 
        base_type(parent, windowFlags) 
    {}

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = NULL) override {
        Q_UNUSED(option)
        Q_UNUSED(widget)
        QRectF rect = this->rect();
        qreal penWidth = qMin(rect.width(), rect.height()) / 16;
        QPointF center = rect.center();
        QPointF centralStep = QPointF(penWidth, penWidth);

        QnScopedPainterPenRollback penRollback(painter, QPen(ptzItemBorderColor, penWidth));
        Q_UNUSED(penRollback)
        QnScopedPainterBrushRollback brushRollback(painter, ptzItemBaseColor);
        Q_UNUSED(brushRollback)

        painter->drawEllipse(rect);
        painter->drawEllipse(QRectF(center - centralStep, center + centralStep));
    }
};


// -------------------------------------------------------------------------- //
// PtzOverlayWidget
// -------------------------------------------------------------------------- //
class PtzOverlayWidget: public QGraphicsWidget {
    typedef QGraphicsWidget base_type;

public:
    PtzOverlayWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0): 
        base_type(parent, windowFlags),
        m_manipulatorWidget(new PtzManipulatorWidget(this)),
        m_arrowItem(new PtzArrowItem(this)),
        m_pointerItem(new PtzPointerItem(this)),
        m_zoomInButton(new PtzZoomButtonWidget(this)),
        m_zoomOutButton(new PtzZoomButtonWidget(this))
    {
        setAcceptedMouseButtons(0);

        m_zoomInButton->setIcon(qnSkin->icon("item/ptz_zoom_in.png"));
        m_zoomOutButton->setIcon(qnSkin->icon("item/ptz_zoom_out.png"));
        m_arrowItem->setOpacity(0.0);
        m_pointerItem->setOpacity(0.0);

        updateLayout();
    }

    PtzManipulatorWidget *manipulatorWidget() const {
        return m_manipulatorWidget;
    }

    PtzArrowItem *arrowItem() const {
        return m_arrowItem;
    }

    PtzPointerItem *pointerItem() const {
        return m_pointerItem;
    }

    PtzZoomButtonWidget *zoomInButton() const {
        return m_zoomInButton;
    }

    PtzZoomButtonWidget *zoomOutButton() const {
        return m_zoomOutButton;
    }

    virtual void setGeometry(const QRectF &rect) override {
        QSizeF oldSize = size();

        base_type::setGeometry(rect);

        if(!qFuzzyCompare(oldSize, size()))
            updateLayout();
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget /* = 0 */) {
        Q_UNUSED(option)
        Q_UNUSED(widget)
        QRectF rect = this->rect();

        QVector<QPointF> crosshairLines; // TODO: cache these?

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

#if 0
        for(int i = 0; i < 4; i++) {
            qreal a = M_PI * (0.25 + 0.5 * i);
            qreal sin = std::sin(a);
            qreal cos = std::cos(a);
            QPointF v0(cos, sin), v1(-sin, cos);

            crosshairLines
                << center + v0 * d1 << center + v0 * d0
                << center + v0 * d0 + v1 * d1 / 2.0 << center + v0 * d0 - v1 * d1 / 2.0;
        }
#endif

        QnScopedPainterPenRollback penRollback(painter, QPen(ptzItemBorderColor));
        painter->drawLines(crosshairLines);
        Q_UNUSED(penRollback)
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

        m_pointerItem->setSize(QSizeF(centralWidth, centralWidth));
    }

private:
    PtzManipulatorWidget *m_manipulatorWidget;
    PtzArrowItem *m_arrowItem;
    PtzPointerItem *m_pointerItem;
    PtzZoomButtonWidget *m_zoomInButton;
    PtzZoomButtonWidget *m_zoomOutButton;
};


// -------------------------------------------------------------------------- //
// PtzInstrument
// -------------------------------------------------------------------------- //
PtzInstrument::PtzInstrument(QObject *parent): 
    base_type(
        makeSet(QEvent::MouseButtonPress, AnimationEvent::Animation),
        makeSet(),
        makeSet(),
        makeSet(QEvent::GraphicsSceneMouseDoubleClick, QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease), 
        parent
    ),
    QnWorkbenchContextAware(parent),
    m_ptzController(context()->instance<QnWorkbenchPtzController>()),
    m_mapperWatcher(context()->instance<QnWorkbenchPtzMapperWatcher>()),
    m_clickDelayMSec(QApplication::doubleClickInterval()),
    m_expansionSpeed(qnGlobals->workbenchUnitSize() / 5.0)
{
    connect(m_ptzController, SIGNAL(positionChanged(const QnVirtualCameraResourcePtr &)), this, SLOT(at_ptzController_positionChanged(const QnVirtualCameraResourcePtr &)));
    connect(m_mapperWatcher, SIGNAL(mapperChanged(const QnVirtualCameraResourcePtr &)), this, SLOT(at_mapperWatcher_mapperChanged(const QnVirtualCameraResourcePtr &)));
    connect(display(), SIGNAL(resourceAdded(const QnResourcePtr &)), this, SLOT(at_display_resourceAdded(const QnResourcePtr &)));
    connect(display(), SIGNAL(resourceAboutToBeRemoved(const QnResourcePtr &)), this, SLOT(at_display_resourceAboutToBeRemoved(const QnResourcePtr &)));
}

PtzInstrument::~PtzInstrument() {
    ensureUninstalled();
}

PtzSplashItem *PtzInstrument::newSplashItem(QGraphicsItem *parentItem) {
    PtzSplashItem *result;
    if(!m_freeAnimations.empty()) {
        result = m_freeAnimations.back().item;
        m_freeAnimations.pop_back();
        result->setParentItem(parentItem);
    } else {
        result = new PtzSplashItem(parentItem);
        connect(result, SIGNAL(destroyed()), this, SLOT(at_splashItem_destroyed()));
    }

    result->setOpacity(0.0);
    result->show();
    return result;
}

PtzOverlayWidget *PtzInstrument::overlayWidget(QnMediaResourceWidget *widget) const {
    return m_dataByWidget[widget].overlayWidget;
}

void PtzInstrument::ensureOverlayWidget(QnMediaResourceWidget *widget) {
    PtzData &data = m_dataByWidget[widget];

    PtzOverlayWidget *overlay = data.overlayWidget;
    if(overlay)
        return;

    overlay = new PtzOverlayWidget();
    overlay->setOpacity(0.0);
    overlay->manipulatorWidget()->setCursor(Qt::SizeAllCursor);
    overlay->zoomInButton()->setTarget(widget);
    overlay->zoomOutButton()->setTarget(widget);
    data.overlayWidget = overlay;

    connect(overlay->zoomInButton(),    SIGNAL(pressed()),  this, SLOT(at_zoomInButton_pressed()));
    connect(overlay->zoomInButton(),    SIGNAL(released()), this, SLOT(at_zoomInButton_released()));
    connect(overlay->zoomOutButton(),   SIGNAL(pressed()),  this, SLOT(at_zoomOutButton_pressed()));
    connect(overlay->zoomOutButton(),   SIGNAL(released()), this, SLOT(at_zoomOutButton_released()));

    widget->addOverlayWidget(overlay, false, false);
}

void PtzInstrument::ensureSelectionItem() {
    if(selectionItem() != NULL)
        return;

    m_selectionItem = new PtzSelectionItem();
    selectionItem()->setOpacity(0.0);
    selectionItem()->setElementSize(qnGlobals->workbenchUnitSize() / 64.0);

    if(scene() != NULL)
        scene()->addItem(selectionItem());
}

void PtzInstrument::updateOverlayWidget() {
    updateOverlayWidget(checked_cast<QnMediaResourceWidget *>(sender()));
}

void PtzInstrument::updateOverlayWidget(QnMediaResourceWidget *widget) {
    bool hasCrosshair = widget->options() & QnResourceWidget::DisplayCrosshair;

    if(hasCrosshair)
        ensureOverlayWidget(widget);

    if(PtzOverlayWidget *overlay = overlayWidget(widget)) {
        opacityAnimator(overlay, 0.5)->animateTo(hasCrosshair ? 1.0 : 0.0);

        overlay->manipulatorWidget()->setVisible(m_dataByWidget[widget].capabilities & Qn::ContinuousPanTiltCapability);
    }
}

void PtzInstrument::updateCapabilities(const QnSecurityCamResourcePtr &resource) {
    foreach(QnResourceWidget *widget, display()->widgets(resource))
        if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
            updateCapabilities(mediaWidget);
}

void PtzInstrument::updateCapabilities(QnMediaResourceWidget *widget) {
    QnVirtualCameraResourcePtr camera = widget->resource().dynamicCast<QnVirtualCameraResource>();
    if(!camera)
        return; /* Just to feel safe. We shouldn't get here. */

    PtzData &data = m_dataByWidget[widget];
    Qn::CameraCapabilities oldCapabilities = data.capabilities;

    data.capabilities = camera->getCameraCapabilities(); 
    if((data.capabilities & Qn::AbsolutePtzCapability) && !m_mapperWatcher->mapper(camera))
        data.capabilities &= ~Qn::AbsolutePtzCapability; /* No mapper? Can't use absolute movement. */

    if(oldCapabilities != data.capabilities)
        updateOverlayWidget(widget);
}

void PtzInstrument::ptzMoveTo(QnMediaResourceWidget *widget, const QPointF &pos) {
    ptzMoveTo(widget, QRectF(pos - toPoint(widget->size() / 2), widget->size()));
}

void PtzInstrument::ptzMoveTo(QnMediaResourceWidget *widget, const QRectF &rect) {
    QnVirtualCameraResourcePtr camera = widget->resource().dynamicCast<QnVirtualCameraResource>();
    const QnPtzSpaceMapper *mapper = m_mapperWatcher->mapper(camera);
    if(!mapper)
        return;

    QVector3D oldPhysicalPosition = m_ptzController->physicalPosition(camera);
    if(qIsNaN(oldPhysicalPosition)) {
        m_ptzController->setMovement(camera, QVector3D());

        PtzData &data = m_dataByWidget[widget];
        data.pendingAbsoluteMove = rect;

        return; 
    }

    qreal sideSize = 36.0 / oldPhysicalPosition.z();
    QVector3D r = sphericalToCartesian<QVector3D>(1.0, oldPhysicalPosition.x() / 180 * M_PI, oldPhysicalPosition.y() / 180 * M_PI);
    QVector3D x = sphericalToCartesian<QVector3D>(1.0, (oldPhysicalPosition.x() + 90) / 180 * M_PI, 0.0) * sideSize;
    QVector3D y = sphericalToCartesian<QVector3D>(1.0, oldPhysicalPosition.x() / 180 * M_PI, (oldPhysicalPosition.y() - 90) / 180 * M_PI) * sideSize;

    QPointF pos = rect.center();
    QVector2D delta = QVector2D(pos - widget->rect().center()) / widget->size().width();
    QVector3D r1 = r + x * delta.x() + y * delta.y();
    QnSphericalPoint<float> spherical = cartesianToSpherical<QVector3D>(r1);

    qreal zoom = widget->rect().width() / rect.width(); /* For 2x zoom we'll get 2.0 here. */
    
    QVector3D newPhysicalPosition = QVector3D(spherical.phi / M_PI * 180, spherical.psi / M_PI * 180, oldPhysicalPosition.z() * zoom);
    QVector3D newLogicalPosition = mapper->toCamera().physicalToLogical(newPhysicalPosition);
    newPhysicalPosition = mapper->toCamera().logicalToPhysical(newLogicalPosition); /* There-and-back mapping ensures bounds. */

    TRACE("PTZ ZOOM(" << newPhysicalPosition.x() - oldPhysicalPosition.x() << ", " << newPhysicalPosition.y() - oldPhysicalPosition.y() << ", " << zoom << "x)");

    m_ptzController->setPhysicalPosition(camera, newPhysicalPosition);
}

void PtzInstrument::ptzUnzoom(QnMediaResourceWidget *widget) {
    QSizeF size = widget->size() * 100;

    ptzMoveTo(widget, QRectF(widget->rect().center() - toPoint(size) / 2, size));
}

void PtzInstrument::ptzMove(QnMediaResourceWidget *widget, const QVector3D &speed, bool instant) {
    PtzData &data = m_dataByWidget[widget];
    data.requestedSpeed = speed;

    if(!instant && (data.currentSpeed - data.requestedSpeed).lengthSquared() < speedUpdateThreshold * speedUpdateThreshold)
        return;

    instant = instant ||
        (qFuzzyIsNull(data.currentSpeed) ^ qFuzzyIsNull(data.requestedSpeed)) || 
        (data.currentSpeed - data.requestedSpeed).lengthSquared() > instantSpeedUpdateThreshold * instantSpeedUpdateThreshold;

    if(instant) {
        QnVirtualCameraResourcePtr camera = widget->resource().dynamicCast<QnVirtualCameraResource>();

        m_ptzController->setMovement(camera, data.requestedSpeed);
        data.currentSpeed = data.requestedSpeed;
        data.pendingAbsoluteMove = QRectF();

        m_movementTimer.stop();
    } else {
        if(!m_movementTimer.isActive())
            m_movementTimer.start(speedUpdateIntervalMSec, this);
    }
}



// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void PtzInstrument::installedNotify() {
    assert(selectionItem() == NULL);

    base_type::installedNotify();
}

void PtzInstrument::aboutToBeDisabledNotify() {
    m_clickTimer.stop();
    m_movementTimer.stop();

    base_type::aboutToBeDisabledNotify();
}

void PtzInstrument::aboutToBeUninstalledNotify() {
    base_type::aboutToBeUninstalledNotify();

    if(selectionItem())
        delete selectionItem();
}

bool PtzInstrument::registeredNotify(QGraphicsItem *item) {
    bool result = base_type::registeredNotify(item);
    if(!result)
        return false;

    if(QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(item)) {
        if(QnVirtualCameraResourcePtr camera = widget->resource().dynamicCast<QnVirtualCameraResource>()) {
            connect(widget, SIGNAL(optionsChanged()), this, SLOT(updateOverlayWidget()));

            PtzData &data = m_dataByWidget[widget];
            data.currentSpeed = data.requestedSpeed = m_ptzController->movement(camera);

            updateCapabilities(widget);
            updateOverlayWidget(widget);

            return true;
        }
    }

    return false;
}

void PtzInstrument::unregisteredNotify(QGraphicsItem *item) {
    base_type::unregisteredNotify(item);

    /* We don't want to use RTTI at this point, so we don't cast to QnMediaResourceWidget. */
    QGraphicsObject *object = item->toGraphicsObject();
    disconnect(object, NULL, this, NULL);

    PtzData &data = m_dataByWidget[object];
    //TODO: #elric fix unused variable warning. Is it really unused?
    m_dataByWidget.remove(object);
}

void PtzInstrument::timerEvent(QTimerEvent *event) {
    if(event->timerId() == m_clickTimer.timerId()) {
        m_clickTimer.stop();

        PtzSplashItem *splashItem = newSplashItem(target());
        splashItem->setSplashType(PtzSplashItem::Circular);
        splashItem->setRect(QRectF(0.0, 0.0, 0.0, 0.0));
        splashItem->setPos(m_clickPos);
        m_activeAnimations.push_back(SplashItemAnimation(splashItem, 1.0, 1.0));

        ptzMoveTo(target(), m_clickPos);
    } else if(event->timerId() == m_movementTimer.timerId()) { 
        if(!target())
            return;

        ptzMove(target(), m_dataByWidget[target()].requestedSpeed);
        m_movementTimer.stop();
    } else {
        base_type::timerEvent(event);
    }
}

bool PtzInstrument::animationEvent(AnimationEvent *event) {
    qreal dt = event->deltaTime() / 1000.0;
    
    for(int i = m_activeAnimations.size() - 1; i >= 0; i--) {
        SplashItemAnimation &animation = m_activeAnimations[i];

        qreal opacity = animation.item->opacity();
        QRectF rect = animation.item->boundingRect();

        qreal opacitySpeed = animation.fadingIn ? 8.0 : -1.0;

        opacity += dt * opacitySpeed * animation.opacityMultiplier;
        if(opacity >= 1.0) {
            animation.fadingIn = false;
            opacity = 1.0;
        } else if(opacity <= 0.0) {
            animation.item->hide();
            m_freeAnimations.push_back(animation);
            m_activeAnimations.removeAt(i);
            continue;
        }
        qreal ds = dt * m_expansionSpeed * animation.expansionMultiplier;
        rect = rect.adjusted(-ds, -ds, ds, ds);

        animation.item->setOpacity(opacity);
        animation.item->setRect(rect);
    }
    
    return false;
}

bool PtzInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *) {
    m_viewport = viewport;

    return false;
}

bool PtzInstrument::mouseDoubleClickEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    return mousePressEvent(item, event);
}

bool PtzInstrument::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    if(!dragProcessor()->isWaiting())
        return false; /* Prevent click-through scenarios. */

    bool clickTimerWasActive = m_clickTimer.isActive();
    m_clickTimer.stop();

    if(event->button() != Qt::LeftButton)
        return false;

    QGraphicsObject *object = item->toGraphicsObject();
    if(!object)
        return false;

    QnMediaResourceWidget *target = checked_cast<QnMediaResourceWidget *>(item);
    if(!(target->options() & QnResourceWidget::ControlPtz))
        return false;

    if(!target->rect().contains(event->pos()))
        return false; /* Ignore clicks on widget frame. */

    PtzManipulatorWidget *manipulator = NULL;
    if(PtzOverlayWidget *overlay = overlayWidget(target)) {
        manipulator = overlay->manipulatorWidget();
        if(!manipulator->isVisible() || !manipulator->rect().contains(manipulator->mapFromItem(item, event->pos())))
            manipulator = NULL;
    }

    if(!manipulator && !(m_dataByWidget[target].capabilities & Qn::AbsolutePtzCapability))
        return false;

    m_isDoubleClick = this->target() == target && clickTimerWasActive && (m_clickPos - event->pos()).manhattanLength() < dragProcessor()->startDragDistance();
    m_target = target;
    m_manipulator = manipulator;

    dragProcessor()->mousePressEvent(item, event);
    
    event->accept();
    return false;
}

void PtzInstrument::startDragProcess(DragInfo *) {
    m_isClick = true;
    emit ptzProcessStarted(target());
}

void PtzInstrument::startDrag(DragInfo *) {
    m_isClick = false;
    m_ptzStartedEmitted = false;

    if(target() == NULL) {
        /* Whoops, already destroyed. */
        dragProcessor()->reset();
        return;
    }

    if(!manipulator()) {
        ensureSelectionItem();
        selectionItem()->setParentItem(target());
        selectionItem()->setViewport(m_viewport.data());
        opacityAnimator(selectionItem())->stop();
        selectionItem()->setOpacity(1.0);
        /* Everything else will be initialized in the first call to drag(). */
    } else {
        manipulator()->setCursor(Qt::BlankCursor);
        target()->setCursor(Qt::BlankCursor);
        opacityAnimator(overlayWidget(target())->arrowItem(), 0.5)->animateTo(1.0);
        /* Everything else will be initialized in the first call to drag(). */
    }

    emit ptzStarted(target());
    m_ptzStartedEmitted = true;
}

void PtzInstrument::dragMove(DragInfo *info) {
    if(target() == NULL) {
        dragProcessor()->reset();
        return;
    }

    if(!manipulator()) {
        ensureSelectionItem();

        QPointF origin = info->mousePressItemPos();
        QPointF corner = bounded(info->mouseItemPos(), target()->rect());
        QRectF rect = QnGeometry::movedInto(
            QnGeometry::expanded(
                QnGeometry::aspectRatio(target()->size()),
                QRectF(origin, corner).normalized(),
                Qt::KeepAspectRatioByExpanding,
                Qt::AlignCenter
            ),
            target()->rect()
        );

        QVector<QPointF> sidePoints;
        sidePoints << origin << corner;

        selectionItem()->setRect(rect);
        selectionItem()->setSidePoints(sidePoints);
    } else {
        QPointF delta = info->mouseItemPos() - target()->rect().center();
        QSizeF size = target()->size();
        qreal scale = qMax(size.width(), size.height()) / 2.0;
        QPointF speed(qBound(-1.0, delta.x() / scale, 1.0), qBound(-1.0, -delta.y() / scale, 1.0));

        /* Adjust speed in case we're dealing with octagonal ptz. */
        const PtzData &data = m_dataByWidget[target()];
        if(data.capabilities & Qn::OctagonalPtzCapability) {
            QnPolarPoint<double> polarSpeed = cartesianToPolar(speed);
            polarSpeed.alpha = qRound(polarSpeed.alpha, M_PI / 4.0); /* Rounded to 45 degrees. */
            speed = polarToCartesian<QPointF>(polarSpeed.r, polarSpeed.alpha);
            if(qFuzzyIsNull(speed.x())) // TODO: #Elric use lower null threshold
                speed.setX(0.0);
            if(qFuzzyIsNull(speed.y()))
                speed.setY(0.0);
        }

        qreal speedMagnitude = length(speed);
        qreal arrowSize = scale / 16.0 * (0.4 + 2.6 * speedMagnitude);

        PtzArrowItem *arrowItem = overlayWidget(target())->arrowItem();
        arrowItem->moveTo(target()->rect().center(), info->mouseItemPos());
        arrowItem->setSize(QSize(arrowSize, arrowSize));

        ptzMove(target(), QVector3D(speed));
    }
}

void PtzInstrument::finishDrag(DragInfo *) {
    if(target()) {
        if(!manipulator()) {
            ensureSelectionItem();
            opacityAnimator(selectionItem(), 4.0)->animateTo(0.0);

            QRectF selectionRect = selectionItem()->boundingRect();

            PtzSplashItem *splashItem = newSplashItem(target());
            splashItem->setSplashType(PtzSplashItem::Rectangular);
            splashItem->setPos(selectionRect.center());
            splashItem->setRect(QRectF(-toPoint(selectionRect.size()) / 2, selectionRect.size()));
            m_activeAnimations.push_back(SplashItemAnimation(splashItem, 1.0, 1.0));

            ptzMoveTo(target(), selectionRect);
        } else {
            manipulator()->setCursor(Qt::SizeAllCursor);
            target()->unsetCursor();
            opacityAnimator(overlayWidget(target())->arrowItem())->animateTo(0.0);
        }
    }

    if(m_ptzStartedEmitted)
        emit ptzFinished(target());
}

void PtzInstrument::finishDragProcess(DragInfo *info) {
    if(target()) {
        if(!manipulator() && m_isClick && m_isDoubleClick) {
            PtzSplashItem *splashItem = newSplashItem(target());
            splashItem->setSplashType(PtzSplashItem::Rectangular);
            splashItem->setPos(target()->rect().center());
            QSizeF size = target()->size() * 1.1;
            splashItem->setRect(QRectF(-toPoint(size) / 2, size));
            m_activeAnimations.push_back(SplashItemAnimation(splashItem, -1.0, 1.0));

            ptzUnzoom(target());
        } else if(!manipulator() && m_isClick) {
            m_clickTimer.start(m_clickDelayMSec, this);
            m_clickPos = info->mousePressItemPos();
        } else if(manipulator()) {
            ptzMove(target(), QVector3D(0.0, 0.0, 0.0));
        }
    }

    emit ptzProcessFinished(target());
}

void PtzInstrument::at_display_resourceAdded(const QnResourcePtr &resource) {
    if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
        connect(camera, SIGNAL(cameraCapabilitiesChanged(const QnSecurityCamResourcePtr &)), this, SLOT(updateCapabilities(const QnSecurityCamResourcePtr &)));
}

void PtzInstrument::at_display_resourceAboutToBeRemoved(const QnResourcePtr &resource) {
    if(QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>())
        disconnect(camera, NULL, this, NULL);
}

void PtzInstrument::at_ptzController_positionChanged(const QnVirtualCameraResourcePtr &camera) {
    if(!target() || target()->resource() != camera)
        return;

    PtzData &data = m_dataByWidget[target()];
    QRectF rect = data.pendingAbsoluteMove;
    if(rect.isNull())
        return;

    data.pendingAbsoluteMove = QRectF();
    ptzMoveTo(target(), rect);
}

void PtzInstrument::at_mapperWatcher_mapperChanged(const QnVirtualCameraResourcePtr &resource) {
    updateCapabilities(resource);
}

void PtzInstrument::at_splashItem_destroyed() {
    PtzSplashItem *item = static_cast<PtzSplashItem *>(sender());

    m_freeAnimations.removeAll(item);
    m_activeAnimations.removeAll(item);
}

void PtzInstrument::at_zoomInButton_pressed() {
    at_zoomButton_activated(1.0);
}

void PtzInstrument::at_zoomInButton_released() {
    at_zoomButton_activated(0.0);
}

void PtzInstrument::at_zoomOutButton_pressed() {
    at_zoomButton_activated(-1.0);
}

void PtzInstrument::at_zoomOutButton_released() {
    at_zoomButton_activated(0.0);
}

void PtzInstrument::at_zoomButton_activated(qreal speed) {
    PtzZoomButtonWidget *button = checked_cast<PtzZoomButtonWidget *>(sender());
    
    if(QnMediaResourceWidget *widget = button->target())
        ptzMove(widget, QVector3D(0.0, 0.0, speed), true);
}
