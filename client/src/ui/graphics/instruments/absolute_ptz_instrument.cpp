#include "absolute_ptz_instrument.h"

#include <cmath>
#include <cassert>
#include <limits>

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

void AbsolutePtzInstrument::setTarget(QnMediaResourceWidget *target) {
    if(this->target() == target)
        return;

    m_target = target;
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

void AbsolutePtzInstrument::ensureSelectionItem() {
    if(selectionItem() != NULL)
        return;

    m_selectionItem = new PtzSelectionItem();
    selectionItem()->setOpacity(0.0);
    selectionItem()->setPen(toTransparent(ptzColor, 0.75));
    selectionItem()->setBrush(toTransparent(ptzColor.lighter(120), 0.25));
    selectionItem()->setElementSize(qnGlobals->workbenchUnitSize() / 64.0);

    if(scene() != NULL)
        scene()->addItem(selectionItem());
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

    QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(item);
    if(widget) {
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
    bool clickTimerWasActive = m_clickTimer.isActive();
    m_clickTimer.stop();

    if(event->button() != Qt::LeftButton)
        return false;

    QnMediaResourceWidget *target = checked_cast<QnMediaResourceWidget *>(item);
    if(!(target->options() & QnResourceWidget::ControlPtz))
        return false;

    if(!target->rect().contains(event->pos()))
        return false; /* Ignore clicks on widget frame. */

    m_isDoubleClick = this->target() == target && clickTimerWasActive && (m_clickPos - event->pos()).manhattanLength() < dragProcessor()->startDragDistance();
    m_target = target;

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

    ensureSelectionItem();
    selectionItem()->setParentItem(target());
    selectionItem()->setViewport(m_viewport.data());
    opacityAnimator(selectionItem())->stop();
    selectionItem()->setOpacity(1.0);
    /* Everything else will be initialized in the first call to drag(). */

    emit ptzStarted(target());
    m_ptzStartedEmitted = true;
}

void AbsolutePtzInstrument::dragMove(DragInfo *info) {
    if(target() == NULL) {
        dragProcessor()->reset();
        return;
    }
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
}

void AbsolutePtzInstrument::finishDrag(DragInfo *) {
    if(target()) {
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

    if(m_ptzStartedEmitted)
        emit ptzFinished(target());
}

void AbsolutePtzInstrument::finishDragProcess(DragInfo *info) {
    if(target() && m_isClick) {
        if(m_isDoubleClick) {
            PtzSplashItem *splashItem = newSplashItem(target());
            splashItem->setSplashType(PtzSplashItem::Rectangular);
            splashItem->setPos(target()->rect().center());
            QSizeF size = target()->size() * 1.1;
            splashItem->setRect(QRectF(-toPoint(size) / 2, size));
            m_activeAnimations.push_back(SplashItemAnimation(splashItem, -1.0, 1.0));

            ptzUnzoom(target());
        } else {
            m_clickTimer.start(m_clickDelayMSec, this);
            m_clickPos = info->mousePressItemPos();
        }
    }

    emit ptzProcessFinished(target());
}

void AbsolutePtzInstrument::at_splashItem_destroyed() {
    PtzSplashItem *item = static_cast<PtzSplashItem *>(sender());

    m_freeAnimations.removeAll(item);
    m_activeAnimations.removeAll(item);
}

