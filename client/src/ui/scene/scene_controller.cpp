#include "scene_controller.h"
#include <cassert>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsGridLayout>
#include <QGLWidget>
#include <QFile>
#include <QIcon>
#include <QAction>
#include <QParallelAnimationGroup>
#include <ui/instruments/instrumentmanager.h>
#include <ui/instruments/handscrollinstrument.h>
#include <ui/instruments/wheelzoominstrument.h>
#include <ui/instruments/rubberbandinstrument.h>
#include <ui/instruments/draginstrument.h>
#include <ui/instruments/contextmenuinstrument.h>
#include <ui/instruments/clickinstrument.h>
#include <ui/instruments/boundinginstrument.h>
#include <ui/instruments/stopinstrument.h>
#include <ui/instruments/stopacceptedinstrument.h>
#include <ui/instruments/forwardinginstrument.h>
#include <ui/instruments/transformlistenerinstrument.h>
#include <ui/instruments/selectioninstrument.h>
#include <ui/instruments/archivedropinstrument.h>
#include <ui/instruments/resizinginstrument.h>
#include <ui/widgets2/centralwidget.h>
#include <ui/widgets2/animatedwidget.h>
#include <ui/widgets2/layoutitem.h>
#include <ui/widgets2/celllayout.h>
#include <ui/view/viewport_animator.h>

#include "display_widget.h"
#include <core/resource/resource_media_layout.h>
#include <camera/camdisplay.h>
#include <camera/camera.h>
#include <core/resource/resource.h>
#include <core/resource/directory_browser.h>
#include <core/resourcemanagment/resource_pool.h>
#include <core/dataprovider/media_streamdataprovider.h>

#include <ui/model/ui_layout.h>
#include <ui/model/ui_layout_item.h>

#include "display_state.h"
#include "display_widget.h"
#include "display_synchronizer.h"
#include "display_grid_mapper.h"

QnSceneController::QnSceneController(QnDisplaySynchronizer *synchronizer, QObject *parent):
    QObject(parent),
    m_synchronizer(synchronizer),
    m_state(synchronizer->state()),
    m_manager(synchronizer->manager()),
    m_scene(synchronizer->scene()),
    m_view(synchronizer->view())
{
    /* Install and configure instruments. */
    ClickInstrument *itemClickInstrument = new ClickInstrument(ClickInstrument::WATCH_ITEM, this);
    ClickInstrument *sceneClickInstrument = new ClickInstrument(ClickInstrument::WATCH_SCENE, this);
    m_handScrollInstrument = new HandScrollInstrument(this);
    m_wheelZoomInstrument = new WheelZoomInstrument(this);
    m_rubberBandInstrument = new RubberBandInstrument(this);
    m_resizingInstrument = new ResizingInstrument(this);

    m_dragInstrument = new DragInstrument(this);
    m_view->setDragMode(QGraphicsView::NoDrag);
    
    m_view->viewport()->setAcceptDrops(true);

    QSet<QEvent::Type> sceneEventTypes = Instrument::makeSet(
        QEvent::GraphicsSceneMousePress,
        QEvent::GraphicsSceneMouseMove,
        QEvent::GraphicsSceneMouseRelease,
        QEvent::GraphicsSceneMouseDoubleClick
    );

    QSet<QEvent::Type> noEventTypes = Instrument::makeSet();

    m_manager->installInstrument(m_resizingInstrument);
    m_manager->installInstrument(new StopInstrument(sceneEventTypes, noEventTypes, Instrument::makeSet(QEvent::Wheel), noEventTypes, this));
    m_manager->installInstrument(sceneClickInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(sceneEventTypes, noEventTypes, noEventTypes, noEventTypes, this));
    m_manager->installInstrument(new SelectionInstrument(this));
    m_manager->installInstrument(new ForwardingInstrument(sceneEventTypes, noEventTypes, noEventTypes, noEventTypes, this));
    m_manager->installInstrument(new TransformListenerInstrument(this));
    m_manager->installInstrument(m_wheelZoomInstrument);
    m_manager->installInstrument(m_dragInstrument);
    m_manager->installInstrument(m_rubberBandInstrument);
    m_manager->installInstrument(m_handScrollInstrument);
    m_manager->installInstrument(new ContextMenuInstrument(this));
    m_manager->installInstrument(itemClickInstrument);
    m_manager->installInstrument(new ArchiveDropInstrument(m_state, this));

    connect(itemClickInstrument,        SIGNAL(clicked(QGraphicsView *, QGraphicsItem *)),                  this,                   SLOT(at_item_clicked(QGraphicsView *, QGraphicsItem *)));
    connect(itemClickInstrument,        SIGNAL(doubleClicked(QGraphicsView *, QGraphicsItem *)),            this,                   SLOT(at_item_doubleClicked(QGraphicsView *, QGraphicsItem *)));
    connect(sceneClickInstrument,       SIGNAL(clicked(QGraphicsView *)),                                   this,                   SLOT(at_scene_clicked(QGraphicsView *)));
    connect(sceneClickInstrument,       SIGNAL(doubleClicked(QGraphicsView *)),                             this,                   SLOT(at_scene_doubleClicked(QGraphicsView *)));
    connect(m_dragInstrument,           SIGNAL(draggingStarted(QGraphicsView *, QList<QGraphicsItem *>)),   this,                   SLOT(at_draggingStarted(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_dragInstrument,           SIGNAL(draggingFinished(QGraphicsView *, QList<QGraphicsItem *>)),  this,                   SLOT(at_draggingFinished(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_resizingInstrument,       SIGNAL(resizingStarted(QGraphicsView *, QGraphicsWidget *)),        this,                   SLOT(at_resizingStarted(QGraphicsView *, QGraphicsWidget *)));
    connect(m_resizingInstrument,       SIGNAL(resizingFinished(QGraphicsView *, QGraphicsWidget *)),       this,                   SLOT(at_resizingFinished(QGraphicsView *, QGraphicsWidget *)));
    connect(m_handScrollInstrument,     SIGNAL(scrollingStarted(QGraphicsView *)),                          m_synchronizer,         SLOT(disableViewportChanges()));
    connect(m_handScrollInstrument,     SIGNAL(scrollingFinished(QGraphicsView *)),                         m_synchronizer,         SLOT(enableViewportChanges()));
    connect(m_synchronizer,             SIGNAL(viewportGrabbed()),                                          m_handScrollInstrument, SLOT(recursiveDisable()));
    connect(m_synchronizer,             SIGNAL(viewportUngrabbed()),                                        m_handScrollInstrument, SLOT(recursiveEnable()));
    connect(m_synchronizer,             SIGNAL(viewportGrabbed()),                                          m_wheelZoomInstrument,  SLOT(recursiveDisable()));
    connect(m_synchronizer,             SIGNAL(viewportUngrabbed()),                                        m_wheelZoomInstrument,  SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *)), m_dragInstrument,       SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *)),m_dragInstrument,       SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *)), m_rubberBandInstrument, SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *)),m_rubberBandInstrument, SLOT(recursiveEnable()));
}

QnSceneController::~QnSceneController() {
    return;
}

void QnSceneController::updateGeometryDelta(QnDisplayWidget *widget) {
    if(widget->entity()->isPinned())
        return;

    QRectF widgetGeometry = widget->geometry();

    QRectF gridGeometry = m_state->gridMapper()->mapFromGrid(widget->entity()->geometry());

    QSizeF step = m_state->gridMapper()->step();
    QRectF geometryDelta = QRectF(
        (widgetGeometry.left()   - gridGeometry.left())   / step.width(),
        (widgetGeometry.top()    - gridGeometry.top())    / step.height(),
        (widgetGeometry.width()  - gridGeometry.width())  / step.width(),
        (widgetGeometry.height() - gridGeometry.height()) / step.height()
    );

    widget->entity()->setGeometryDelta(geometryDelta);
}

void QnSceneController::at_resizingStarted(QGraphicsView *, QGraphicsWidget *) {
    qDebug("RESIZING STARTED");
}

void QnSceneController::at_resizingFinished(QGraphicsView *view, QGraphicsWidget *item) {
    qDebug("RESIZING FINISHED");

    QnDisplayWidget *widget = dynamic_cast<QnDisplayWidget *>(item);
    if(widget == NULL)
        return;

    QRect newRect = m_state->gridMapper()->mapToGrid(widget->geometry());
    QSet<QnUiLayoutItem *> entities = m_state->model()->items(newRect);
    entities.remove(widget->entity());
    if (entities.empty()) {
        widget->entity()->setGeometry(newRect);
        updateGeometryDelta(widget);
    }

    m_synchronizer->synchronize(widget->entity());
}

void QnSceneController::at_draggingStarted(QGraphicsView *, QList<QGraphicsItem *> items) {
    qDebug("DRAGGING STARTED");

    foreach(QGraphicsItem *item, items) {
        m_synchronizer->bringToFront(item);
        m_synchronizer->setLayer(item, QnDisplaySynchronizer::FRONT_LAYER);
    }
}

void QnSceneController::at_draggingFinished(QGraphicsView *view, QList<QGraphicsItem *> items) {
    qDebug("DRAGGING FINISHED");

    /* Get entities and drag delta. */
    QList<QnDisplayWidget *> widgets;
    QList<QnUiLayoutItem *> entities;
    QPoint delta;
    foreach (QGraphicsItem *item, items) {
        QnDisplayWidget *widget = dynamic_cast<QnDisplayWidget *>(item);
        if(widget == NULL)
            continue;

        if(entities.empty())
            delta = m_state->gridMapper()->mapToGrid(widget->geometry()).topLeft() - widget->entity()->geometry().topLeft();

        widgets.push_back(widget);
        entities.push_back(widget->entity());
    }
    if(entities.empty())
        return;

    bool success = false;
    if(delta == QPoint(0, 0)) {
        success = true;
    } else {
        QList<QRect> geometries;
        bool finished = false;

        /* Handle single widget case. */
        if(entities.size() == 1) {
            QnUiLayoutItem *draggedEntity = entities[0];

            /* Find entity that dragged entity was dropped on. */ 
            QPoint cursorPos = QCursor::pos();
            QnUiLayoutItem *replacedEntity = m_state->model()->item(m_state->gridMapper()->mapToGrid(view->mapToScene(view->mapFromGlobal(cursorPos))));

            /* Switch places if dropping smaller one on a bigger one. */
            if(replacedEntity != NULL && replacedEntity != draggedEntity && draggedEntity->isPinned()) {
                QSizeF draggedSize = draggedEntity->geometry().size();
                QSizeF replacedSize = replacedEntity->geometry().size();
                if(replacedSize.width() >= draggedSize.width() && replacedSize.height() >= draggedSize.height()) {
                    entities.push_back(replacedEntity);
                    geometries.push_back(replacedEntity->geometry());
                    geometries.push_back(draggedEntity->geometry());
                    finished = true;
                }
            }
        }

        /* Handle all other cases. */
        if(!finished) {
            QList<QRect> replacedGeometries;
            foreach (QnUiLayoutItem *entity, entities) {
                QRect geometry = entity->geometry().adjusted(delta.x(), delta.y(), delta.x(), delta.y());
                geometries.push_back(geometry);
                if(entity->isPinned())
                    replacedGeometries.push_back(geometry);
            }

            QList<QnUiLayoutItem *> replacedEntities = m_state->model()->items(replacedGeometries).subtract(entities.toSet()).toList();
            replacedGeometries.clear();
            foreach (QnUiLayoutItem *entity, replacedEntities)
                replacedGeometries.push_back(entity->geometry().adjusted(-delta.x(), -delta.y(), -delta.x(), -delta.y()));

            entities.append(replacedEntities);
            geometries.append(replacedGeometries);
            finished = true;
        }

        success = m_state->model()->moveItems(entities, geometries);
    }

    /* Adjust geometry deltas if everything went fine. */
    if(success)
        foreach(QnDisplayWidget *widget, widgets)
            updateGeometryDelta(widget);

    /* Re-sync everything. */
    foreach(QnUiLayoutItem *entity, entities)
        m_synchronizer->synchronize(entity);
}

void QnSceneController::at_item_clicked(QGraphicsView *, QGraphicsItem *item) {
    QnDisplayWidget *widget = dynamic_cast<QnDisplayWidget *>(item);
    if(widget == NULL)
        return;

    QnUiLayoutItem *entity = widget->entity();
    m_state->setSelectedEntity(m_state->selectedEntity() == entity ? NULL : entity);
}

void QnSceneController::at_item_doubleClicked(QGraphicsView *, QGraphicsItem *item) {
    QnDisplayWidget *widget = dynamic_cast<QnDisplayWidget *>(item);
    if(widget == NULL)
        return;

    QnUiLayoutItem *entity = widget->entity();
    if(m_state->zoomedEntity() == entity) {
        QRectF viewportGeometry = m_synchronizer->viewportGeometry();
        QRectF zoomedEntityGeometry = m_synchronizer->zoomedEntityGeometry();

        if(contains(zoomedEntityGeometry.size(), viewportGeometry.size()) && !qFuzzyCompare(viewportGeometry, zoomedEntityGeometry)) {
            m_state->setZoomedEntity(NULL);
            m_state->setZoomedEntity(entity);
        } else {
            m_state->setZoomedEntity(NULL);
            m_state->setSelectedEntity(NULL);
        }
    } else {
        m_state->setZoomedEntity(entity);
    }
}

void QnSceneController::at_scene_clicked(QGraphicsView *) {
    m_state->setSelectedEntity(NULL);
}

void QnSceneController::at_scene_doubleClicked(QGraphicsView *) {
    m_state->setZoomedEntity(NULL);
    m_synchronizer->fitInView();
}

