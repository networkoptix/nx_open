#include "display_controller.h"
#include <cassert>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGLWidget>

#include <ui/animation/viewport_animator.h>

#include <ui/graphics/instruments/instrumentmanager.h>
#include <ui/graphics/instruments/handscrollinstrument.h>
#include <ui/graphics/instruments/wheelzoominstrument.h>
#include <ui/graphics/instruments/rubberbandinstrument.h>
#include <ui/graphics/instruments/draginstrument.h>
#include <ui/graphics/instruments/clickinstrument.h>
#include <ui/graphics/instruments/boundinginstrument.h>
#include <ui/graphics/instruments/stopinstrument.h>
#include <ui/graphics/instruments/stopacceptedinstrument.h>
#include <ui/graphics/instruments/forwardinginstrument.h>
#include <ui/graphics/instruments/transformlistenerinstrument.h>
#include <ui/graphics/instruments/selectioninstrument.h>
#include <ui/graphics/instruments/archivedropinstrument.h>
#include <ui/graphics/instruments/resizinginstrument.h>

#include <ui/graphics/items/display_widget.h>

#include <ui/model/layout_model.h>
#include <ui/model/resource_item_model.h>
#include <ui/model/layout_grid_mapper.h>

#include <file_processor.h>

#include "display_state.h"
#include "layout_display.h"

QnDisplayController::QnDisplayController(QnLayoutDisplay *synchronizer, QObject *parent):
    QObject(parent),
    m_synchronizer(synchronizer),
    m_state(synchronizer->state()),
    m_manager(synchronizer->manager()),
    m_scene(synchronizer->scene()),
    m_view(synchronizer->view())
{
    /* Install and configure instruments. */
    ClickInstrument *itemClickInstrument = new ClickInstrument(Qt::LeftButton, Instrument::ITEM, this);
    ClickInstrument *sceneClickInstrument = new ClickInstrument(Qt::LeftButton, Instrument::SCENE, this);
    m_handScrollInstrument = new HandScrollInstrument(this);
    m_wheelZoomInstrument = new WheelZoomInstrument(this);
    m_rubberBandInstrument = new RubberBandInstrument(this);
    m_resizingInstrument = new ResizingInstrument(this);
    m_archiveDropInstrument = new ArchiveDropInstrument(this, this);

    m_dragInstrument = new DragInstrument(this);
    m_view->setDragMode(QGraphicsView::NoDrag);
    
    m_view->viewport()->setAcceptDrops(true);

    QSet<QEvent::Type> mouseEventTypes = Instrument::makeSet(
        QEvent::GraphicsSceneMousePress,
        QEvent::GraphicsSceneMouseMove,
        QEvent::GraphicsSceneMouseRelease,
        QEvent::GraphicsSceneMouseDoubleClick
    );

    /* Item instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::ITEM, mouseEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::ITEM, mouseEventTypes, this));
    m_manager->installInstrument(itemClickInstrument);

    /* Scene instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::SCENE, mouseEventTypes, this));
    m_manager->installInstrument(sceneClickInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::SCENE, mouseEventTypes, this));
    m_manager->installInstrument(new SelectionInstrument(this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::SCENE, mouseEventTypes, this));

    /* View/viewport instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::VIEWPORT, Instrument::makeSet(QEvent::Wheel), this));
    m_manager->installInstrument(m_resizingInstrument);
    m_manager->installInstrument(new TransformListenerInstrument(this));
    m_manager->installInstrument(m_wheelZoomInstrument);
    m_manager->installInstrument(m_dragInstrument);
    m_manager->installInstrument(m_rubberBandInstrument);
    m_manager->installInstrument(m_handScrollInstrument);
    m_manager->installInstrument(m_archiveDropInstrument);

    connect(itemClickInstrument,        SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const QPoint &)),      this,                   SLOT(at_item_clicked(QGraphicsView *, QGraphicsItem *)));
    connect(itemClickInstrument,        SIGNAL(doubleClicked(QGraphicsView *, QGraphicsItem *, const QPoint &)),this,                   SLOT(at_item_doubleClicked(QGraphicsView *, QGraphicsItem *)));
    connect(sceneClickInstrument,       SIGNAL(clicked(QGraphicsView *, const QPoint &)),                       this,                   SLOT(at_scene_clicked(QGraphicsView *)));
    connect(sceneClickInstrument,       SIGNAL(doubleClicked(QGraphicsView *, const QPoint &)),                 this,                   SLOT(at_scene_doubleClicked(QGraphicsView *)));
    connect(m_dragInstrument,           SIGNAL(draggingStarted(QGraphicsView *, QList<QGraphicsItem *>)),       this,                   SLOT(at_draggingStarted(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_dragInstrument,           SIGNAL(draggingFinished(QGraphicsView *, QList<QGraphicsItem *>)),      this,                   SLOT(at_draggingFinished(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_resizingInstrument,       SIGNAL(resizingStarted(QGraphicsView *, QGraphicsWidget *)),            this,                   SLOT(at_resizingStarted(QGraphicsView *, QGraphicsWidget *)));
    connect(m_resizingInstrument,       SIGNAL(resizingFinished(QGraphicsView *, QGraphicsWidget *)),           this,                   SLOT(at_resizingFinished(QGraphicsView *, QGraphicsWidget *)));
    connect(m_handScrollInstrument,     SIGNAL(scrollingStarted(QGraphicsView *)),                              m_synchronizer,         SLOT(disableViewportChanges()));
    connect(m_handScrollInstrument,     SIGNAL(scrollingFinished(QGraphicsView *)),                             m_synchronizer,         SLOT(enableViewportChanges()));
    connect(m_synchronizer,             SIGNAL(viewportGrabbed()),                                              m_handScrollInstrument, SLOT(recursiveDisable()));
    connect(m_synchronizer,             SIGNAL(viewportUngrabbed()),                                            m_handScrollInstrument, SLOT(recursiveEnable()));
    connect(m_synchronizer,             SIGNAL(viewportGrabbed()),                                              m_wheelZoomInstrument,  SLOT(recursiveDisable()));
    connect(m_synchronizer,             SIGNAL(viewportUngrabbed()),                                            m_wheelZoomInstrument,  SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *)),     m_dragInstrument,       SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *)),    m_dragInstrument,       SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *)),     m_rubberBandInstrument, SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *)),    m_rubberBandInstrument, SLOT(recursiveEnable()));

    connect(m_synchronizer,             SIGNAL(viewportGrabbed()),                                              this, SLOT(at_viewportGrabbed()));
    connect(m_synchronizer,             SIGNAL(viewportUngrabbed()),                                            this, SLOT(at_viewportUngrabbed()));
}

QnDisplayController::~QnDisplayController() {
    return;
}

void QnDisplayController::drop(const QUrl &url, const QPoint &gridPos, bool checkUrls) {
    drop(url.toLocalFile(), gridPos, checkUrls);
}

void QnDisplayController::drop(const QList<QUrl> &urls, const QPoint &gridPos, bool checkUrls) {
    QList<QString> files;
    foreach(const QUrl &url, urls)
        files.push_back(url.toLocalFile());
    drop(files, gridPos, checkUrls);
}

void QnDisplayController::drop(const QString &file, const QPoint &gridPos, bool checkFiles) {
    QList<QString> files;
    files.push_back(file);
    drop(files, gridPos, checkFiles);
}

void QnDisplayController::drop(const QList<QString> &files, const QPoint &gridPos, bool checkFiles) {
    if(!checkFiles) {
        dropInternal(files, gridPos);
    } else {
        QStringList checkedFiles;
        foreach(const QString &file, files)
            QnFileProcessor::findAcceptedFiles(file, &checkedFiles);
        if(files.empty())
            return;

        dropInternal(checkedFiles, gridPos);
    }
}

void QnDisplayController::dropInternal(const QList<QString> &files, const QPoint &gridPos) {
    QnResourceList resources = QnFileProcessor::creareResourcesForFiles(files);

    QRect geometry(gridPos, QSize(1, 1));
    foreach(QnResourcePtr resource, resources) {
        QnLayoutItemModel *item = new QnLayoutItemModel(resource->getUniqueId());
        item->setGeometry(geometry);

        m_state->layout()->addItem(item);
        if(!item->isPinned()) {
            /* Place already taken, pick closest one. */
            QRect newGeometry = m_state->layout()->closestFreeSlot(geometry.topLeft(), geometry.size());
            m_state->layout()->pinItem(item, newGeometry);
        }
    }
}

void QnDisplayController::updateGeometryDelta(QnDisplayWidget *widget) {
    if(widget->item()->isPinned())
        return;

    QRectF widgetGeometry = widget->geometry();

    QRectF gridGeometry = m_state->mapper()->mapFromGrid(widget->item()->geometry());

    QSizeF step = m_state->mapper()->step();
    QRectF geometryDelta = QRectF(
        (widgetGeometry.left()   - gridGeometry.left())   / step.width(),
        (widgetGeometry.top()    - gridGeometry.top())    / step.height(),
        (widgetGeometry.width()  - gridGeometry.width())  / step.width(),
        (widgetGeometry.height() - gridGeometry.height()) / step.height()
    );

    widget->item()->setGeometryDelta(geometryDelta);
}

void QnDisplayController::at_resizingStarted(QGraphicsView *, QGraphicsWidget *item) {
    qDebug("RESIZING STARTED");

    m_synchronizer->bringToFront(item);
}

void QnDisplayController::at_resizingFinished(QGraphicsView *, QGraphicsWidget *item) {
    qDebug("RESIZING FINISHED");

    QnDisplayWidget *widget = dynamic_cast<QnDisplayWidget *>(item);
    if(widget == NULL)
        return;

    QRect newRect = m_state->mapper()->mapToGrid(widget->geometry());
    QSet<QnLayoutItemModel *> entities = m_state->layout()->items(newRect);
    entities.remove(widget->item());
    if (entities.empty()) {
        widget->item()->setGeometry(newRect);
        updateGeometryDelta(widget);
    }

    m_synchronizer->synchronize(widget->item());
}

void QnDisplayController::at_draggingStarted(QGraphicsView *, QList<QGraphicsItem *> items) {
    qDebug("DRAGGING STARTED");

    foreach(QGraphicsItem *item, items) {
        m_synchronizer->bringToFront(item);
        m_synchronizer->setLayer(item, QnLayoutDisplay::FRONT_LAYER);
    }
}

void QnDisplayController::at_draggingFinished(QGraphicsView *view, QList<QGraphicsItem *> items) {
    qDebug("DRAGGING FINISHED");

    /* Get models and drag delta. */
    QList<QnDisplayWidget *> widgets;
    QList<QnLayoutItemModel *> models;
    QPoint delta;
    foreach (QGraphicsItem *item, items) {
        QnDisplayWidget *widget = dynamic_cast<QnDisplayWidget *>(item);
        if(widget == NULL)
            continue;

        if(models.empty())
            delta = m_state->mapper()->mapToGrid(widget->geometry()).topLeft() - widget->item()->geometry().topLeft();

        widgets.push_back(widget);
        models.push_back(widget->item());
    }
    if(models.empty())
        return;

    bool success = false;
    if(delta == QPoint(0, 0)) {
        success = true;
    } else {
        QList<QRect> geometries;
        bool finished = false;

        /* Handle single widget case. */
        if(models.size() == 1) {
            QnLayoutItemModel *draggedModel = models[0];

            /* Find item that dragged item was dropped on. */ 
            QPoint cursorPos = QCursor::pos();
            QnLayoutItemModel *replacedModel = m_state->layout()->item(m_state->mapper()->mapToGrid(view->mapToScene(view->mapFromGlobal(cursorPos))));

            /* Switch places if dropping smaller one on a bigger one. */
            if(replacedModel != NULL && replacedModel != draggedModel && draggedModel->isPinned()) {
                QSizeF draggedSize = draggedModel->geometry().size();
                QSizeF replacedSize = replacedModel->geometry().size();
                if(replacedSize.width() >= draggedSize.width() && replacedSize.height() >= draggedSize.height()) {
                    models.push_back(replacedModel);
                    geometries.push_back(replacedModel->geometry());
                    geometries.push_back(draggedModel->geometry());
                    finished = true;
                }
            }
        }

        /* Handle all other cases. */
        if(!finished) {
            QList<QRect> replacedGeometries;
            foreach (QnLayoutItemModel *model, models) {
                QRect geometry = model->geometry().adjusted(delta.x(), delta.y(), delta.x(), delta.y());
                geometries.push_back(geometry);
                if(model->isPinned())
                    replacedGeometries.push_back(geometry);
            }

            QList<QnLayoutItemModel *> replacedModels = m_state->layout()->items(replacedGeometries).subtract(models.toSet()).toList();
            replacedGeometries.clear();
            foreach (QnLayoutItemModel *model, replacedModels)
                replacedGeometries.push_back(model->geometry().adjusted(-delta.x(), -delta.y(), -delta.x(), -delta.y()));

            models.append(replacedModels);
            geometries.append(replacedGeometries);
            finished = true;
        }

        success = m_state->layout()->moveItems(models, geometries);
    }

    /* Adjust geometry deltas if everything went fine. */
    if(success)
        foreach(QnDisplayWidget *widget, widgets)
            updateGeometryDelta(widget);

    /* Re-sync everything. */
    foreach(QnLayoutItemModel *model, models)
        m_synchronizer->synchronize(model);
}

void QnDisplayController::at_item_clicked(QGraphicsView *, QGraphicsItem *item) {
    qDebug("CLICKED");

    QnDisplayWidget *widget = dynamic_cast<QnDisplayWidget *>(item);
    if(widget == NULL)
        return;

    QnLayoutItemModel *model = widget->item();
    m_state->setSelectedItem(m_state->selectedItem() == model ? NULL : model);
}

void QnDisplayController::at_item_doubleClicked(QGraphicsView *, QGraphicsItem *item) {
    qDebug("DOUBLE CLICKED");

    QnDisplayWidget *widget = dynamic_cast<QnDisplayWidget *>(item);
    if(widget == NULL)
        return;

    QnLayoutItemModel *model = widget->item();
    if(m_state->zoomedItem() == model) {
        QRectF viewportGeometry = m_synchronizer->viewportGeometry();
        QRectF zoomedItemGeometry = m_synchronizer->itemGeometry(m_state->zoomedItem());

        if(contains(zoomedItemGeometry.size(), viewportGeometry.size()) && !qFuzzyCompare(viewportGeometry, zoomedItemGeometry)) {
            m_state->setZoomedItem(NULL);
            m_state->setZoomedItem(model);
        } else {
            m_state->setZoomedItem(NULL);
            m_state->setSelectedItem(NULL);
        }
    } else {
        m_state->setZoomedItem(model);
    }
}

void QnDisplayController::at_scene_clicked(QGraphicsView *) {
    qDebug("SCENE CLICKED");

    m_state->setSelectedItem(NULL);
}

void QnDisplayController::at_scene_doubleClicked(QGraphicsView *) {
    qDebug("SCENE DOUBLE CLICKED");

    m_state->setZoomedItem(NULL);
    m_synchronizer->fitInView();
}

void QnDisplayController::at_viewportGrabbed() {
    qDebug("VIEWPORT GRABBED");
}

void QnDisplayController::at_viewportUngrabbed() {
    qDebug("VIEWPORT UNGRABBED");
}
