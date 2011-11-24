#include "workbench_controller.h"
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
#include <ui/graphics/instruments/selectionfixupinstrument.h>
#include <ui/graphics/instruments/archivedropinstrument.h>
#include <ui/graphics/instruments/resizinginstrument.h>
#include <ui/graphics/instruments/uielementsinstrument.h>

#include <ui/graphics/items/display_widget.h>

#include <file_processor.h>

#include "workbench_layout.h"
#include "workbench_item.h"
#include "workbench_grid_mapper.h"
#include "workbench.h"
#include "workbench_manager.h"


#include <ui/videoitem/navigationitem.h>
#include <camera/camdisplay.h>


QnWorkbenchController::QnWorkbenchController(QnWorkbenchManager *synchronizer, QObject *parent):
    QObject(parent),
    m_synchronizer(synchronizer),
    m_manager(synchronizer->instrumentManager()),
    m_focusedItem(NULL)
{
    /* Install and configure instruments. */
    ClickInstrument *itemClickInstrument = new ClickInstrument(Qt::LeftButton, Instrument::ITEM, this);
    ClickInstrument *sceneClickInstrument = new ClickInstrument(Qt::LeftButton, Instrument::SCENE, this);
    m_handScrollInstrument = new HandScrollInstrument(this);
    m_wheelZoomInstrument = new WheelZoomInstrument(this);
    m_rubberBandInstrument = new RubberBandInstrument(this);
    m_resizingInstrument = new ResizingInstrument(this);
    m_archiveDropInstrument = new ArchiveDropInstrument(this, this);
    m_uiElementsInstrument = new UiElementsInstrument(this);
    BoundingInstrument *boundingInstrument = m_synchronizer->boundingInstrument();
    m_dragInstrument = new DragInstrument(this);

    m_rubberBandInstrument->setRubberBandZValue(m_synchronizer->layerZValue(QnWorkbenchManager::EFFECTS_LAYER));

    QSet<QEvent::Type> mouseEventTypes = Instrument::makeSet(
        QEvent::GraphicsSceneMousePress,
        QEvent::GraphicsSceneMouseMove,
        QEvent::GraphicsSceneMouseRelease,
        QEvent::GraphicsSceneMouseDoubleClick
    );

    /* Item instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::ITEM, mouseEventTypes, this));
    m_manager->installInstrument(new SelectionFixupInstrument(this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::ITEM, mouseEventTypes, this));
    m_manager->installInstrument(itemClickInstrument);

    /* Scene instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::SCENE, mouseEventTypes, this));
    m_manager->installInstrument(sceneClickInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::SCENE, mouseEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::SCENE, mouseEventTypes, this));

    /* View/viewport instruments. */
    m_manager->installInstrument(m_uiElementsInstrument, InstallationMode::INSTALL_FIRST);
    m_manager->installInstrument(new StopInstrument(Instrument::VIEWPORT, Instrument::makeSet(QEvent::Wheel), this));
    m_manager->installInstrument(m_resizingInstrument);
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
    connect(m_handScrollInstrument,     SIGNAL(scrollingStarted(QGraphicsView *)),                              boundingInstrument,     SLOT(dontEnforcePosition(QGraphicsView *)));
    connect(m_handScrollInstrument,     SIGNAL(scrollingFinished(QGraphicsView *)),                             boundingInstrument,     SLOT(enforcePosition(QGraphicsView *)));
    connect(m_synchronizer,             SIGNAL(viewportGrabbed()),                                              m_handScrollInstrument, SLOT(recursiveDisable()));
    connect(m_synchronizer,             SIGNAL(viewportUngrabbed()),                                            m_handScrollInstrument, SLOT(recursiveEnable()));
    connect(m_synchronizer,             SIGNAL(viewportGrabbed()),                                              m_wheelZoomInstrument,  SLOT(recursiveDisable()));
    connect(m_synchronizer,             SIGNAL(viewportUngrabbed()),                                            m_wheelZoomInstrument,  SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *)),     m_dragInstrument,       SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *)),    m_dragInstrument,       SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *)),     m_rubberBandInstrument, SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *)),    m_rubberBandInstrument, SLOT(recursiveEnable()));

    connect(m_synchronizer,             SIGNAL(viewportGrabbed()),                                              this,                   SLOT(at_viewportGrabbed()));
    connect(m_synchronizer,             SIGNAL(viewportUngrabbed()),                                            this,                   SLOT(at_viewportUngrabbed()));

    /* Create controls. */
    QGraphicsWidget *controlsWidget = m_uiElementsInstrument->widget();
    m_synchronizer->setLayer(controlsWidget, QnWorkbenchManager::UI_ELEMENTS_LAYER);

    QGraphicsLinearLayout *verticalLayout = new QGraphicsLinearLayout(Qt::Vertical);
    verticalLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    controlsWidget->setLayout(verticalLayout);

    m_navigationItem = new NavigationItem();
    m_navigationItem->setParentItem(controlsWidget);

    verticalLayout->addStretch(100);

    m_navigationItem->graphicsWidget()->setParentItem(NULL);
    verticalLayout->addItem(m_navigationItem->graphicsWidget());

    /* Connect to workbench. */
    connect(workbench(),                SIGNAL(focusedItemChanged()),                                           this,                   SLOT(at_workbench_focusedItemChanged()));
}

QnWorkbenchController::~QnWorkbenchController() {
    return;
}

QnWorkbench *QnWorkbenchController::workbench() const {
    return m_synchronizer->workbench();
}

QnWorkbenchLayout *QnWorkbenchController::layout() const {
    return m_synchronizer->workbench()->layout();
}

QnWorkbenchGridMapper *QnWorkbenchController::mapper() const {
    return m_synchronizer->workbench()->mapper();
}

void QnWorkbenchController::drop(const QUrl &url, const QPoint &gridPos, bool findAccepted) {
    drop(url.toLocalFile(), gridPos, findAccepted);
}

void QnWorkbenchController::drop(const QList<QUrl> &urls, const QPoint &gridPos, bool findAccepted) {
    QList<QString> files;
    foreach(const QUrl &url, urls)
        files.push_back(url.toLocalFile());
    drop(files, gridPos, findAccepted);
}

void QnWorkbenchController::drop(const QString &file, const QPoint &gridPos, bool findAccepted) {
    QList<QString> files;
    files.push_back(file);
    drop(files, gridPos, findAccepted);
}

void QnWorkbenchController::drop(const QList<QString> &files, const QPoint &gridPos, bool findAccepted) {
    QList<QString> validFiles;
    if(!findAccepted) {
        validFiles = files;
    } else {
        foreach(const QString &file, files)
            QnFileProcessor::findAcceptedFiles(file, &validFiles);
    }

    if(validFiles.empty())
        return;

    drop(QnFileProcessor::createResourcesForFiles(validFiles), gridPos);
}

void QnWorkbenchController::drop(const QnResourceList &resources, const QPoint &gridPos) {
    foreach(const QnResourcePtr &resource, resources)
        drop(resource, gridPos);
}

void QnWorkbenchController::drop(const QnResourcePtr &resource, const QPoint &gridPos) {
    QRect geometry(gridPos, QSize(1, 1));
    QnWorkbenchItem *item = new QnWorkbenchItem(resource->getUniqueId());
    item->setGeometry(geometry);

    layout()->addItem(item);
    if(!item->isPinned()) {
        /* Place already taken, pick closest one. */
        QRect newGeometry = layout()->closestFreeSlot(geometry.topLeft(), geometry.size());
        layout()->pinItem(item, newGeometry);
    }
}

void QnWorkbenchController::updateGeometryDelta(QnDisplayWidget *widget) {
    if(widget->item()->isPinned())
        return;

    QRectF widgetGeometry = widget->geometry();

    QRectF gridGeometry = mapper()->mapFromGrid(widget->item()->geometry());

    QSizeF step = mapper()->step();
    QRectF geometryDelta = QRectF(
        (widgetGeometry.left()   - gridGeometry.left())   / step.width(),
        (widgetGeometry.top()    - gridGeometry.top())    / step.height(),
        (widgetGeometry.width()  - gridGeometry.width())  / step.width(),
        (widgetGeometry.height() - gridGeometry.height()) / step.height()
    );

    widget->item()->setGeometryDelta(geometryDelta);
}

void QnWorkbenchController::at_resizingStarted(QGraphicsView *, QGraphicsWidget *item) {
    qDebug("RESIZING STARTED");

    m_synchronizer->bringToFront(item);
}

void QnWorkbenchController::at_resizingFinished(QGraphicsView *, QGraphicsWidget *item) {
    qDebug("RESIZING FINISHED");

    QnDisplayWidget *widget = dynamic_cast<QnDisplayWidget *>(item);
    if(widget == NULL)
        return;

    QRect newRect = mapper()->mapToGrid(widget->geometry());
    QSet<QnWorkbenchItem *> entities = layout()->items(newRect);
    entities.remove(widget->item());
    if (entities.empty()) {
        widget->item()->setGeometry(newRect);
        updateGeometryDelta(widget);
    }

    m_synchronizer->synchronize(widget->item());
}

void QnWorkbenchController::at_draggingStarted(QGraphicsView *, QList<QGraphicsItem *> items) {
    qDebug("DRAGGING STARTED");

    /* Bring to front preserving relative order. */
    m_synchronizer->bringToFront(items);
    m_synchronizer->setLayer(items, QnWorkbenchManager::FRONT_LAYER);
}

void QnWorkbenchController::at_draggingFinished(QGraphicsView *view, QList<QGraphicsItem *> items) {
    qDebug("DRAGGING FINISHED");

    /* Get models and drag delta. */
    QList<QnDisplayWidget *> widgets;
    QList<QnWorkbenchItem *> models;
    QPoint delta;
    foreach (QGraphicsItem *item, items) {
        QnDisplayWidget *widget = dynamic_cast<QnDisplayWidget *>(item);
        if(widget == NULL)
            continue;

        if(models.empty())
            delta = mapper()->mapToGrid(widget->geometry()).topLeft() - widget->item()->geometry().topLeft();

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
            QnWorkbenchItem *draggedModel = models[0];

            /* Find item that dragged item was dropped on. */ 
            QPoint cursorPos = QCursor::pos();
            QnWorkbenchItem *replacedModel = layout()->item(mapper()->mapToGrid(view->mapToScene(view->mapFromGlobal(cursorPos))));

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
            foreach (QnWorkbenchItem *model, models) {
                QRect geometry = model->geometry().adjusted(delta.x(), delta.y(), delta.x(), delta.y());
                geometries.push_back(geometry);
                if(model->isPinned())
                    replacedGeometries.push_back(geometry);
            }

            QList<QnWorkbenchItem *> replacedModels = layout()->items(replacedGeometries).subtract(models.toSet()).toList();
            replacedGeometries.clear();
            foreach (QnWorkbenchItem *model, replacedModels)
                replacedGeometries.push_back(model->geometry().adjusted(-delta.x(), -delta.y(), -delta.x(), -delta.y()));

            models.append(replacedModels);
            geometries.append(replacedGeometries);
            finished = true;
        }

        success = layout()->moveItems(models, geometries);
    }

    /* Adjust geometry deltas if everything went fine. */
    if(success)
        foreach(QnDisplayWidget *widget, widgets)
            updateGeometryDelta(widget);

    /* Re-sync everything. */
    foreach(QnWorkbenchItem *model, models)
        m_synchronizer->synchronize(model);
}

void QnWorkbenchController::at_item_clicked(QGraphicsView *, QGraphicsItem *item) {
    qDebug("CLICKED");

    QnDisplayWidget *widget = dynamic_cast<QnDisplayWidget *>(item);
    if(widget == NULL)
        return;

    QnWorkbenchItem *workbenchItem = widget->item();
    workbench()->setRaisedItem(workbench()->raisedItem() == workbenchItem ? NULL : workbenchItem);
    workbench()->setFocusedItem(workbenchItem);
}

void QnWorkbenchController::at_item_doubleClicked(QGraphicsView *, QGraphicsItem *item) {
    qDebug("DOUBLE CLICKED");

    QnDisplayWidget *widget = dynamic_cast<QnDisplayWidget *>(item);
    if(widget == NULL)
        return;

    QnWorkbenchItem *model = widget->item();
    if(workbench()->zoomedItem() == model) {
        QRectF viewportGeometry = m_synchronizer->viewportGeometry();
        QRectF zoomedItemGeometry = m_synchronizer->itemGeometry(workbench()->zoomedItem());

        if(contains(zoomedItemGeometry.size(), viewportGeometry.size()) && !qFuzzyCompare(viewportGeometry, zoomedItemGeometry)) {
            workbench()->setZoomedItem(NULL);
            workbench()->setZoomedItem(model);
        } else {
            workbench()->setZoomedItem(NULL);
            workbench()->setRaisedItem(NULL);
        }
    } else {
        workbench()->setZoomedItem(model);
    }
}

void QnWorkbenchController::at_scene_clicked(QGraphicsView *) {
    if(workbench() == NULL)
        return;

    workbench()->setRaisedItem(NULL);
}

void QnWorkbenchController::at_scene_doubleClicked(QGraphicsView *) {
    if(workbench() == NULL)
        return;

    workbench()->setZoomedItem(NULL);
    m_synchronizer->fitInView();
}

void QnWorkbenchController::at_viewportGrabbed() {
    qDebug("VIEWPORT GRABBED");
}

void QnWorkbenchController::at_viewportUngrabbed() {
    qDebug("VIEWPORT UNGRABBED");
}

void QnWorkbenchController::at_workbench_focusedItemChanged() {
    QnWorkbenchItem *oldFocusedItem = m_focusedItem;
    m_focusedItem = workbench()->focusedItem();

    /* Stop audio on previously focused item. */
    CLCamDisplay *oldCamDisplay = m_synchronizer->camDisplay(oldFocusedItem);
    if(oldCamDisplay != NULL)
        oldCamDisplay->playAudio(false);

    /* Update navigation item's target. */
    m_navigationItem->setVideoCamera(m_synchronizer->camera(m_focusedItem));

    /* Play audio on newly focused item. */
    CLCamDisplay *newCamDisplay = m_synchronizer->camDisplay(m_focusedItem);
    if(newCamDisplay != NULL)
        newCamDisplay->playAudio(true);
}