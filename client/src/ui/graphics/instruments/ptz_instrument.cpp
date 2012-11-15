#include "ptz_instrument.h"

#include <cassert>
#include <limits>

#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QVector2D>

#include <utils/common/checked_cast.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/fuzzy.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <api/media_server_connection.h>

#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/animation/animation_event.h>
#include <ui/style/globals.h>

#include "selection_item.h"

namespace {
    const QColor ptzColor = qnGlobals->ptzColor();

} // anonymous namespace


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
        return m_rect;
    }

    void setBoundingRect(const QRectF &rect) {
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
    m_ptzItemZValue(0.0),
    m_expansionSpeed(qnGlobals->workbenchUnitSize() / 5.0)
{}

PtzInstrument::~PtzInstrument() {
    ensureUninstalled();
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
    }

    result->setOpacity(1.0);
    result->show();
    m_activeSplashItems.push_back(result);
    return result;
}

void PtzInstrument::ensureSelectionItem() {
    if(selectionItem() != NULL)
        return;

    m_selectionItem = new SelectionItem();
    selectionItem()->setVisible(false);
    selectionItem()->setColor(SelectionItem::Border, toTransparent(ptzColor, 0.75));
    selectionItem()->setColor(SelectionItem::Base, toTransparent(ptzColor.lighter(120), 0.5));

    if(scene() != NULL)
        scene()->addItem(selectionItem());
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
        item->setBoundingRect(rect);
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

void PtzInstrument::startDrag(DragInfo *info) {
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
    selectionItem()->setVisible(true);
    selectionItem()->setOrigin(info->mousePressItemPos());
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

    selectionItem()->setCorner(bounded(info->mouseItemPos(), target()->rect()));
}

void PtzInstrument::finishDrag(DragInfo *) {
    if(target()) {
        ensureSelectionItem();
        selectionItem()->setVisible(false);
        selectionItem()->setParentItem(NULL);

        QRectF selectionRect = selectionItem()->boundingRect();

        PtzSplashItem *splash = newSplashItem(target());
        splash->setSplashType(PtzSplashItem::Rectangular);
        splash->setPos(selectionRect.center());
        selectionRect.moveCenter(QPointF(0.0, 0.0));
        splash->setBoundingRect(selectionRect);
    }

    if(m_ptzStartedEmitted)
        emit ptzFinished(target());
}

void PtzInstrument::finishDragProcess(DragInfo *info) {
    if(target() && m_isClick) {
        PtzSplashItem *splash = newSplashItem(target());
        splash->setSplashType(PtzSplashItem::Circular);
        splash->setBoundingRect(QRectF(0.0, 0.0, 0.0, 0.0));
        splash->setPos(info->mousePressItemPos());
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


