#include "ptz_instrument.h"

#include <cmath>
#include <cassert>
#include <limits>

#include <QtGui/QGraphicsSceneMouseEvent>

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
#include <ui/workbench/watchers/workbench_ptz_cameras_watcher.h>

#include "selection_item.h"
#include "utils/settings.h"

namespace {
    const QColor ptzColor = qnGlobals->ptzColor();

    /* 
     Get width from here:
     http://en.wikipedia.org/wiki/Image_sensor_format
     Note: 1/2.8" is 5.2mm x 3.9mm, D=6.5mm.

     Calculate width-based crop factor C = 36/W.

     Multiply focal lengths by width-based crop factor to get width-based equivalent focal length.
    */

} // anonymous namespace


// -------------------------------------------------------------------------- //
// QnPtzInformation
// -------------------------------------------------------------------------- //
class QnPtzInformation {
public:
    QnPtzInformation() {}
    QnPtzInformation(const QnVectorSpaceMapper &mapper): fromCameraMapper(mapper), toCameraMapper(mapper) {}
    QnPtzInformation(const QnVectorSpaceMapper &fromCameraMapper, const QnVectorSpaceMapper &toCameraMapper): fromCameraMapper(fromCameraMapper), toCameraMapper(toCameraMapper) {}

    QnVectorSpaceMapper fromCameraMapper;
    QnVectorSpaceMapper toCameraMapper;
};


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
            QPointF center = m_rect.center();

            for(int i = 0; i < 4; i++) {
                QPainterPath path;

                path = QPainterPath();
                path.moveTo(center);
                path.lineTo(points[i]);
                path.lineTo(points[i + 1]);
                path.closeSubpath();
                painter->fillPath(path, m_brushes[i]);
            }
        }
    }
        
private:
    /** Splash type. */
    SplashType m_splashType;

    /** Brushes that are used for painting. 0 for circular, 1-4 for rectangular splash. */
    QBrush m_brushes[5];

    /** Bounding rectangle of the splash. */
    QRectF m_rect;
};


// -------------------------------------------------------------------------- //
// PtzInstrument
// -------------------------------------------------------------------------- //
PtzInstrument::PtzInstrument(QObject *parent): 
    base_type(
        makeSet(QEvent::MouseButtonPress, AnimationEvent::Animation),
        makeSet(),
        makeSet(),
        makeSet(QEvent::GraphicsSceneMousePress, QEvent::GraphicsSceneMouseMove, QEvent::GraphicsSceneMouseRelease), 
        parent
    ),
    QnWorkbenchContextAware(parent),
    m_ptzItemZValue(0.0),
    m_expansionSpeed(qnGlobals->workbenchUnitSize() / 5.0)
{
    QnWorkbenchPtzCamerasWatcher *watcher = context()->instance<QnWorkbenchPtzCamerasWatcher>();
    connect(watcher, SIGNAL(ptzCameraAdded(const QnVirtualCameraResourcePtr &)), this, SLOT(at_ptzCameraWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &)));
    connect(watcher, SIGNAL(ptzCameraRemoved(const QnVirtualCameraResourcePtr &)), this, SLOT(at_ptzCameraWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &)));
    foreach(const QnVirtualCameraResourcePtr &camera, watcher->ptzCameras())
        at_ptzCameraWatcher_ptzCameraAdded(camera);

    // TODO: make configurable.
    QVector<QPair<qreal, qreal> > toCameraY;
    toCameraY.push_back(qMakePair(-0.5, -90.0));
    toCameraY.push_back(qMakePair(0.0, 0.0));
    toCameraY.push_back(qMakePair(1.0, 20.0));


    const qreal cropFactor = 7.92;//6.92;
    m_infoByModel[QLatin1String("Q6035-E")] = new QnPtzInformation(
        QnVectorSpaceMapper(
            QnScalarSpaceMapper(-1, 1, 0, 360, Qn::PeriodicExtrapolation),
            QnScalarSpaceMapper(0.111, -0.5, 20, -90, Qn::ConstantExtrapolation),
            QnScalarSpaceMapper(0, 1, 4.7 * cropFactor, 94 * cropFactor, Qn::ConstantExtrapolation)
        ),
        QnVectorSpaceMapper(
            QnScalarSpaceMapper(-1, 1, 0, 360, Qn::PeriodicExtrapolation),
            QnScalarSpaceMapper(toCameraY, Qn::ConstantExtrapolation),
            QnScalarSpaceMapper(0, 1, 4.7 * cropFactor, 94 * cropFactor, Qn::ConstantExtrapolation)
        )
    );
}

PtzInstrument::~PtzInstrument() {
    ensureUninstalled();

    qDeleteAll(m_infoByModel);
}

void PtzInstrument::setPtzItemZValue(qreal ptzItemZValue) {
    m_ptzItemZValue = ptzItemZValue;
}

void PtzInstrument::setTarget(QnMediaResourceWidget *target) {
    if(this->target() == target)
        return;

    m_target = target;
}

PtzSplashItem *PtzInstrument::newSplashItem(QGraphicsItem *parentItem) {
    PtzSplashItem *result;
    if(!m_freeSplashItems.empty()) {
        result = m_freeSplashItems.back();
        m_freeSplashItems.pop_back();
        result->setParentItem(parentItem);
    } else {
        result = new PtzSplashItem(parentItem);
        connect(result, SIGNAL(destroyed()), this, SLOT(at_splashItem_destroyed()));
    }

    result->setOpacity(1.0);
    result->show();
    m_activeSplashItems.push_back(result);
    return result;
}

void PtzInstrument::ensureSelectionItem() {
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

void PtzInstrument::ptzMoveTo(QnMediaResourceWidget *widget, const QPointF &pos) {
    QnVirtualCameraResourcePtr camera = widget->resource().dynamicCast<QnVirtualCameraResource>();

    const QnPtzInformation *info = m_infoByModel[camera->getModel()];
    if(!info)
        return;

    QVector3D oldLogicalPosition = m_ptzPositionByCamera.value(camera);
    QVector3D oldPhysicalPosition = info->toCameraMapper.logicalToPhysical(oldLogicalPosition);

    qreal focalLength = oldPhysicalPosition.z();
    qreal sideSize = 36.0 / focalLength;

    QVector3D r = sphericalToCartesian<QVector3D>(1.0, oldPhysicalPosition.x() / 180 * M_PI, oldPhysicalPosition.y() / 180 * M_PI);
    QVector3D x = QVector3D::crossProduct(QVector3D(0, 0, 1), r).normalized() * sideSize;
    QVector3D y = QVector3D::crossProduct(x, r).normalized() * sideSize;
    
    QVector2D delta = QVector2D(pos - widget->rect().center()) / widget->size().width();
    QVector3D r1 = r + x * delta.x() + y * delta.y();
    QnSphericalPoint<float> spherical = cartesianToSpherical<QVector3D>(r1);
    if(spherical.phi < 0)
        spherical.phi += M_PI * 2.0;

    QVector3D newPhysicalPosition = QVector3D(spherical.phi / M_PI * 180, spherical.psi / M_PI * 180,  oldPhysicalPosition.z());
    QVector3D newLogicalPosition = info->toCameraMapper.physicalToLogical(newPhysicalPosition);

    qDebug() << "PTZ MOVETO(" << newLogicalPosition << ")";
    qDebug() << "PTZ ROTATE(" << newPhysicalPosition.x() - oldPhysicalPosition.x() << ", " << newPhysicalPosition.y() - oldPhysicalPosition.y() << ")";

    m_ptzPositionByCamera[camera] = newLogicalPosition;

    QnMediaServerResourcePtr server = resourcePool()->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();
    
    if(qnSettings->debugCounter() % 2) {
        int handle = server->apiConnection()->asyncPtzMoveTo(camera, newLogicalPosition.x(), newLogicalPosition.y(), newLogicalPosition.z(), this, SLOT(at_ptzMoveTo_replyReceived(int, int)));
        m_cameraByHandle[handle] = camera;
    } else {
        int handle = server->apiConnection()->asyncPtzGetPos(camera, this, SLOT(at_ptzGetPos_replyReceived(int, qreal, qreal, qreal, int)));
        m_cameraByHandle[handle] = camera;
    }
}


void PtzInstrument::ptzMoveTo(QnMediaResourceWidget *widget, const QRectF &rect) {

}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void PtzInstrument::installedNotify() {
    assert(selectionItem() == NULL);

    base_type::installedNotify();
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

    QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(item);
    if(widget) {
        connect(widget, SIGNAL(optionsChanged()), this, SLOT(at_target_optionsChanged()));
        return true;
    } else {
        return false;
    }
}

void PtzInstrument::unregisteredNotify(QGraphicsItem *item) {
    /* We don't want to use RTTI at this point, so we don't cast to QnMediaResourceWidget. */
    QGraphicsObject *object = item->toGraphicsObject();
    disconnect(object, NULL, this, NULL);
}

bool PtzInstrument::animationEvent(AnimationEvent *event) {
    qreal dt = event->deltaTime() / 1000.0;
    
    for(int i = m_activeSplashItems.size() - 1; i >= 0; i--) {
        PtzSplashItem *item = m_activeSplashItems[i];

        qreal opacity = item->opacity();
        QRectF rect = item->boundingRect();

        opacity -= dt * 1.0;
        if(opacity <= 0.0) {
            item->hide();
            m_activeSplashItems.removeAt(i);
            m_freeSplashItems.push_back(item);
            continue;
        }
        qreal ds = dt * m_expansionSpeed;
        rect = rect.adjusted(-ds, -ds, ds, ds);

        item->setOpacity(opacity);
        item->setRect(rect);
    }
    
    return false;
}

bool PtzInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *) {
    m_viewport = viewport;

    return false;
}

bool PtzInstrument::mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    QnMediaResourceWidget *target = checked_cast<QnMediaResourceWidget *>(item);
    if(!(target->options() & QnResourceWidget::ControlPtz))
        return false;

    if(!target->rect().contains(event->pos()))
        return false; /* Ignore clicks on widget frame. */

    m_target = target;
    dragProcessor()->mousePressEvent(item, event);
    
    event->accept();
    return false;

    /*if(QnNetworkResourcePtr camera = target->resource().dynamicCast<QnNetworkResource>()) {
        if(QnMediaServerResourcePtr server = camera->resourcePool()->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>()) {
            setServerSpeed(QVector3D(), true);

            m_camera = camera;
            m_connection = server->apiConnection();
            
            dragProcessor()->mousePressEvent(item, event);
        }
    }
    */
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

    ensureSelectionItem();
    selectionItem()->setParentItem(target());
    selectionItem()->setViewport(m_viewport.data());
    opacityAnimator(selectionItem())->stop();
    selectionItem()->setOpacity(1.0);
    /* Everything else will be initialized in the first call to drag(). */

    emit ptzStarted(target());
    m_ptzStartedEmitted = true;
}

void PtzInstrument::dragMove(DragInfo *info) {
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

void PtzInstrument::finishDrag(DragInfo *) {
    if(target()) {
        ensureSelectionItem();
        opacityAnimator(selectionItem(), 4.0)->animateTo(0.0);

        QRectF selectionRect = selectionItem()->boundingRect();

        PtzSplashItem *splash = newSplashItem(target());
        splash->setSplashType(PtzSplashItem::Rectangular);
        splash->setPos(selectionRect.center());
        splash->setRect(QRectF(-toPoint(selectionRect.size()) / 2, selectionRect.size()));

        ptzMoveTo(target(), selectionRect);
    }

    if(m_ptzStartedEmitted)
        emit ptzFinished(target());
}

void PtzInstrument::finishDragProcess(DragInfo *info) {
    if(target() && m_isClick) {
        PtzSplashItem *splash = newSplashItem(target());
        splash->setSplashType(PtzSplashItem::Circular);
        splash->setRect(QRectF(0.0, 0.0, 0.0, 0.0));
        splash->setPos(info->mousePressItemPos());

        ptzMoveTo(target(), info->mousePressItemPos());
    }

    emit ptzProcessFinished(target());
}

void PtzInstrument::at_replyReceived(int status, int handle) {
    Q_UNUSED(status);
    Q_UNUSED(handle);
}

void PtzInstrument::at_target_optionsChanged() {
    //if(sender() == target())
        //updatePtzItemOpacity();
}

void PtzInstrument::at_splashItem_destroyed() {
    PtzSplashItem *item = static_cast<PtzSplashItem *>(sender());

    m_freeSplashItems.removeAll(item);
    m_activeSplashItems.removeAll(item);
}

void PtzInstrument::at_ptzCameraWatcher_ptzCameraAdded(const QnVirtualCameraResourcePtr &camera) {
    QnMediaServerResourcePtr server = resourcePool()->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();

    int handle = server->apiConnection()->asyncPtzGetPos(camera, this, SLOT(at_ptzGetPos_replyReceived(int, qreal, qreal, qreal, int)));
    m_cameraByHandle[handle] = camera;
}

void PtzInstrument::at_ptzCameraWatcher_ptzCameraRemoved(const QnVirtualCameraResourcePtr &camera) {
    m_ptzPositionByCamera.remove(camera);

    for(QHash<int, QnVirtualCameraResourcePtr>::iterator pos = m_cameraByHandle.begin(); pos != m_cameraByHandle.end();) {
        if(pos.value() == camera) {
            pos = m_cameraByHandle.erase(pos);
        } else {
            pos++;
        }
    }
}

void PtzInstrument::at_ptzGetPos_replyReceived(int status, qreal xPos, qreal yPox, qreal zoomPos, int handle) {
    if(status != 0) {
        qnWarning("Could not get PTZ position from camera.");
        return;
    }

    QnVirtualCameraResourcePtr camera = m_cameraByHandle.value(handle);
    if(!camera)
        return;

    m_cameraByHandle.remove(handle);
    m_ptzPositionByCamera[camera] = QVector3D(xPos, yPox, zoomPos);


    qDebug() << "PTZ POS" << camera->getName() << "=" << QVector3D(xPos, yPox, zoomPos);
}

void PtzInstrument::at_ptzMoveTo_replyReceived(int status, int handle) {
    return;
}

