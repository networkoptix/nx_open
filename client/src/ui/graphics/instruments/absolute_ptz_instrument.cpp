#include "absolute_ptz_instrument.h"

#include <cmath>
#include <cassert>
#include <limits>

#include <QtCore/QVariant>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QApplication>

#include <utils/common/checked_cast.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/fuzzy.h>
#include <utils/common/space_mapper.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <api/media_server_connection.h>

#include <ui/common/coordinate_transformations.h>
#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/animation/animation_event.h>
#include <ui/style/globals.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_ptz_controller.h>
#include <ui/workbench/watchers/workbench_ptz_cameras_watcher.h>

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

    const QColor ptzBorderColor = toTransparent(ptzColor, 0.75);
    const QColor ptzBaseColor = toTransparent(ptzColor.lighter(120), 0.25);

    const qreal instantSpeedUpdateThreshold = 0.1;
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
    {}

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
    PtzArrowItem(QGraphicsItem *parent = NULL): base_type(parent) {}

private:

};



// -------------------------------------------------------------------------- //
// PtzManipulatorWidget
// -------------------------------------------------------------------------- //
class PtzManipulatorWidget: public Instrumented<QGraphicsWidget> {
    typedef Instrumented<QGraphicsWidget> base_type;

public:
    PtzManipulatorWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0): 
        base_type(parent, windowFlags) 
    {
        setCursor(Qt::SizeAllCursor);
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = NULL) override {
        QRectF rect = this->rect();
        qreal penWidth = qMin(rect.width(), rect.height()) / 16;
        QPointF center = rect.center();
        QPointF centralStep = QPointF(penWidth, penWidth);

        QnScopedPainterPenRollback penRollback(painter, QPen(ptzBorderColor, penWidth));
        QnScopedPainterBrushRollback brushRollback(painter, ptzBaseColor);

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
        m_manipulatorWidget(new PtzManipulatorWidget(this))
    {
        updateLayout();
    }

    PtzManipulatorWidget *manipulatorWidget() const {
        return m_manipulatorWidget;
    }

    virtual void setGeometry(const QRectF &rect) override {
        QSizeF oldSize = size();

        base_type::setGeometry(rect);

        if(!qFuzzyCompare(oldSize, size()))
            updateLayout();
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget /* = 0 */) {
        painter->setPen(Qt::green);
        painter->drawRect(rect());
    }

private:
    void updateLayout() {
        /* We're doing manual layout of child items as this is an overlay widget and
         * we don't want layouts to mess up widget's size constraints. */

        QRectF rect = this->rect();
        QPointF center = rect.center();
        qreal centralWidth = qMin(rect.width(), rect.height()) / 32;
        QPointF centralStep(centralWidth, centralWidth);

        m_manipulatorWidget->setGeometry(QRectF(center - centralStep, center + centralStep));
    }

private:
    PtzManipulatorWidget *m_manipulatorWidget;
};

Q_DECLARE_METATYPE(PtzOverlayWidget *);


// -------------------------------------------------------------------------- //
// AbsolutePtzInstrument
// -------------------------------------------------------------------------- //
AbsolutePtzInstrument::AbsolutePtzInstrument(QObject *parent): 
    base_type(
        makeSet(QEvent::MouseButtonPress, AnimationEvent::Animation),
        makeSet(),
        makeSet(),
        makeSet(QEvent::GraphicsSceneMouseDoubleClick, QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease), 
        parent
    ),
    QnWorkbenchContextAware(parent),
    m_ptzController(context()->instance<QnWorkbenchPtzController>()),
    m_clickDelayMSec(QApplication::doubleClickInterval()),
    m_expansionSpeed(qnGlobals->workbenchUnitSize() / 5.0)
{}

AbsolutePtzInstrument::~AbsolutePtzInstrument() {
    ensureUninstalled();
}

PtzSplashItem *AbsolutePtzInstrument::newSplashItem(QGraphicsItem *parentItem) {
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

PtzOverlayWidget *AbsolutePtzInstrument::overlayWidget(QnMediaResourceWidget *widget) const {
    return m_overlayByWidget.value(widget);
}

void AbsolutePtzInstrument::ensureOverlayWidget(QnMediaResourceWidget *widget) {
    PtzOverlayWidget *overlay = m_overlayByWidget.value(widget);
    if(overlay)
        return;

    overlay = new PtzOverlayWidget();
    m_overlayByWidget[widget] = overlay;

    widget->addOverlayWidget(overlay, false, false);
}

void AbsolutePtzInstrument::ensureSelectionItem() {
    if(selectionItem() != NULL)
        return;

    m_selectionItem = new PtzSelectionItem();
    selectionItem()->setOpacity(0.0);
    selectionItem()->setPen(ptzBorderColor);
    selectionItem()->setBrush(ptzBaseColor);
    selectionItem()->setElementSize(qnGlobals->workbenchUnitSize() / 64.0);

    if(scene() != NULL)
        scene()->addItem(selectionItem());
}

void AbsolutePtzInstrument::updateOverlayWidget() {
    updateOverlayWidget(checked_cast<QnMediaResourceWidget *>(sender()));
}

void AbsolutePtzInstrument::updateOverlayWidget(QnMediaResourceWidget *widget) {
    if(widget->options() & QnResourceWidget::DisplayCrosshair)
        ensureOverlayWidget(widget);
}

void AbsolutePtzInstrument::ptzMoveTo(QnMediaResourceWidget *widget, const QPointF &pos) {
    ptzMoveTo(widget, QRectF(pos - toPoint(widget->size() / 2), widget->size()));
}


void AbsolutePtzInstrument::ptzMoveTo(QnMediaResourceWidget *widget, const QRectF &rect) {
    QnVirtualCameraResourcePtr camera = widget->resource().dynamicCast<QnVirtualCameraResource>();
    const QnPtzSpaceMapper *mapper = m_ptzController->mapper(camera);
    if(!mapper)
        return;

    QVector3D oldPhysicalPosition = m_ptzController->physicalPosition(camera);

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
    QVector3D newLogicalPosition = mapper->toCamera.physicalToLogical(newPhysicalPosition);
    newPhysicalPosition = mapper->toCamera.logicalToPhysical(newLogicalPosition); /* There-and-back mapping ensures bounds. */

    TRACE("PTZ ZOOM(" << newPhysicalPosition.x() - oldPhysicalPosition.x() << ", " << newPhysicalPosition.y() - oldPhysicalPosition.y() << ", " << zoom << "x)");

    m_ptzController->setPhysicalPosition(camera, newPhysicalPosition);
}

void AbsolutePtzInstrument::ptzUnzoom(QnMediaResourceWidget *widget) {
    QSizeF size = widget->size() * 100;

    ptzMoveTo(widget, QRectF(widget->rect().center() - toPoint(size) / 2, size));
}

void AbsolutePtzInstrument::ptzMove(QnMediaResourceWidget *widget, const QVector3D &speed) {
    PtzSpeed &ptzSpeed = m_speedByWidget[widget];

    ptzSpeed.requested = speed;

    bool send = 
        (qFuzzyIsNull(ptzSpeed.current) ^ qFuzzyIsNull(ptzSpeed.requested)) || 
        (ptzSpeed.current - ptzSpeed.requested).lengthSquared() > instantSpeedUpdateThreshold * instantSpeedUpdateThreshold;

    if(!send) 
        return;

    QnVirtualCameraResourcePtr camera = widget->resource().dynamicCast<QnVirtualCameraResource>();

    m_ptzController->setMovement(camera, ptzSpeed.requested);
    ptzSpeed.current = ptzSpeed.requested;
}



// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void AbsolutePtzInstrument::installedNotify() {
    assert(selectionItem() == NULL);

    base_type::installedNotify();
}

void AbsolutePtzInstrument::aboutToBeDisabledNotify() {
    m_clickTimer.stop();

    base_type::aboutToBeDisabledNotify();
}

void AbsolutePtzInstrument::aboutToBeUninstalledNotify() {
    base_type::aboutToBeUninstalledNotify();

    if(selectionItem())
        delete selectionItem();
}

bool AbsolutePtzInstrument::registeredNotify(QGraphicsItem *item) {
    bool result = base_type::registeredNotify(item);
    if(!result)
        return false;

    if(QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(item)) {
        connect(widget, SIGNAL(optionsChanged()), this, SLOT(updateOverlayWidget()));
        updateOverlayWidget(widget);

        QnVirtualCameraResourcePtr camera = widget->resource().dynamicCast<QnVirtualCameraResource>();
        if(m_ptzController->mapper(camera)) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

void AbsolutePtzInstrument::unregisteredNotify(QGraphicsItem *item) {
    /* We don't want to use RTTI at this point, so we don't cast to QnMediaResourceWidget. */
    QGraphicsObject *object = item->toGraphicsObject();
    disconnect(object, NULL, this, NULL);

    m_overlayByWidget.remove(object);
    m_speedByWidget.remove(object);
}

void AbsolutePtzInstrument::timerEvent(QTimerEvent *event) {
    if(event->timerId() == m_clickTimer.timerId()) {
        m_clickTimer.stop();

        PtzSplashItem *splashItem = newSplashItem(target());
        splashItem->setSplashType(PtzSplashItem::Circular);
        splashItem->setRect(QRectF(0.0, 0.0, 0.0, 0.0));
        splashItem->setPos(m_clickPos);
        m_activeAnimations.push_back(SplashItemAnimation(splashItem, 1.0, 1.0));

        ptzMoveTo(target(), m_clickPos);
    } else {
        base_type::timerEvent(event);
    }
}

bool AbsolutePtzInstrument::animationEvent(AnimationEvent *event) {
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

bool AbsolutePtzInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *) {
    m_viewport = viewport;

    return false;
}

bool AbsolutePtzInstrument::mouseDoubleClickEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    return mousePressEvent(item, event);
}

bool AbsolutePtzInstrument::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
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
        if(!manipulator->rect().contains(manipulator->mapFromItem(item, event->pos())))
            manipulator = NULL;
    }

    m_isDoubleClick = this->target() == target && clickTimerWasActive && (m_clickPos - event->pos()).manhattanLength() < dragProcessor()->startDragDistance();
    m_target = target;
    m_manipulator = manipulator;

    dragProcessor()->mousePressEvent(item, event);
    
    event->accept();
    return false;
}

void AbsolutePtzInstrument::startDragProcess(DragInfo *) {
    m_isClick = true;
    emit ptzProcessStarted(target());
}

void AbsolutePtzInstrument::startDrag(DragInfo *) {
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
    }

    emit ptzStarted(target());
    m_ptzStartedEmitted = true;
}

void AbsolutePtzInstrument::dragMove(DragInfo *info) {
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
        QPointF delta = info->mouseItemPos() - info->mousePressItemPos();

        QSizeF size = target()->size();
        qreal scale = qMax(size.width(), size.height()) / 2.0;

        ptzMove(target(), QVector3D(qBound(-1.0, delta.x() / scale, 1.0), qBound(-1.0, -delta.y() / scale, 1.0), 0.0));
    }
}

void AbsolutePtzInstrument::finishDrag(DragInfo *) {
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
        }
    }

    if(m_ptzStartedEmitted)
        emit ptzFinished(target());
}

void AbsolutePtzInstrument::finishDragProcess(DragInfo *info) {
    if(target()) {
        if(m_isClick && m_isDoubleClick) {
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
        } else if(manipulator() && m_isClick) {
            ptzMove(target(), QVector3D(0.0, 0.0, 0.0));
        }
    }

    emit ptzProcessFinished(target());
}

void AbsolutePtzInstrument::at_splashItem_destroyed() {
    PtzSplashItem *item = static_cast<PtzSplashItem *>(sender());

    m_freeAnimations.removeAll(item);
    m_activeAnimations.removeAll(item);
}

