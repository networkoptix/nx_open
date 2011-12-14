#include "workbench_controller.h"
#include <cassert>
#include <cmath> /* For std::floor. */
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGLWidget>
#include <QGraphicsLinearLayout>
#include <QAction>
#include <QMenu>

#include <core/resourcemanagment/security_cam_resource.h>

#include <camera/resource_display.h>
#include <camera/camdisplay.h>

#include <ui/animation/viewport_animator.h>

#include <ui/graphics/instruments/instrumentmanager.h>
#include <ui/graphics/instruments/handscrollinstrument.h>
#include <ui/graphics/instruments/wheelzoominstrument.h>
#include <ui/graphics/instruments/rubberbandinstrument.h>
#include <ui/graphics/instruments/draginstrument.h>
#include <ui/graphics/instruments/rotationinstrument.h>
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
#include <ui/graphics/instruments/resizehoverinstrument.h>
#include <ui/graphics/instruments/signalinginstrument.h>

#include <ui/graphics/items/resource_widget.h>

#include <ui/videoitem/navigationitem.h>

#include <file_processor.h>

#include "grid_walker.h"
#include "workbench_layout.h"
#include "workbench_item.h"
#include "workbench_grid_mapper.h"
#include "workbench.h"
#include "workbench_display.h"

namespace {
    QAction *newAction(const QString &text, const QString &shortcut, QObject *parent = NULL) {
        QAction *result = new QAction(text, parent);
        result->setShortcut(shortcut);
        return result;
    }


} // anonymous namespace

QnWorkbenchController::QnWorkbenchController(QnWorkbenchDisplay *display, QObject *parent):
    QObject(parent),
    m_display(display),
    m_manager(display->instrumentManager())
{
    std::memset(m_widgetByRole, 0, sizeof(m_widgetByRole));

    QEvent::Type mouseEventTypeArray[] = {
        QEvent::GraphicsSceneMousePress,
        QEvent::GraphicsSceneMouseMove,
        QEvent::GraphicsSceneMouseRelease,
        QEvent::GraphicsSceneMouseDoubleClick,
        QEvent::GraphicsSceneHoverEnter,
        QEvent::GraphicsSceneHoverMove,
        QEvent::GraphicsSceneHoverLeave
    };

    Instrument::EventTypeSet mouseEventTypes = Instrument::makeSet(mouseEventTypeArray);
    Instrument::EventTypeSet wheelEventTypes = Instrument::makeSet(QEvent::GraphicsSceneWheel);

    /* Install and configure instruments. */
    ClickInstrument *itemClickInstrument = new ClickInstrument(Qt::LeftButton, Instrument::ITEM, this);
    ClickInstrument *sceneClickInstrument = new ClickInstrument(Qt::LeftButton, Instrument::SCENE, this);
    m_handScrollInstrument = new HandScrollInstrument(this);
    m_wheelZoomInstrument = new WheelZoomInstrument(this);
    m_rubberBandInstrument = new RubberBandInstrument(this);
    m_rotationInstrument = new RotationInstrument(this);
    m_resizingInstrument = new ResizingInstrument(this);
    m_archiveDropInstrument = new ArchiveDropInstrument(this, this);
    m_uiElementsInstrument = new UiElementsInstrument(this);
    BoundingInstrument *boundingInstrument = m_display->boundingInstrument();
    m_dragInstrument = new DragInstrument(this);
    ForwardingInstrument *itemMouseForwardingInstrument = new ForwardingInstrument(Instrument::ITEM, mouseEventTypes, this);
    SignalingInstrument *itemContextMenuInstrument = new SignalingInstrument(Instrument::ITEM, Instrument::makeSet(QEvent::GraphicsSceneContextMenu), this);
    SelectionFixupInstrument *selectionFixupInstrument = new SelectionFixupInstrument(this);

    m_rubberBandInstrument->setRubberBandZValue(m_display->layerZValue(QnWorkbenchDisplay::EFFECTS_LAYER));
    m_rotationInstrument->setRotationItemZValue(m_display->layerZValue(QnWorkbenchDisplay::EFFECTS_LAYER));
    m_resizingInstrument->setEffectiveDistance(5);

    /* Item instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::ITEM, mouseEventTypes, this));
    m_manager->installInstrument(m_resizingInstrument->resizeHoverInstrument());
    m_manager->installInstrument(selectionFixupInstrument);
    m_manager->installInstrument(itemMouseForwardingInstrument);
    m_manager->installInstrument(selectionFixupInstrument->preForwardingInstrument());
    m_manager->installInstrument(itemClickInstrument);
    m_manager->installInstrument(itemContextMenuInstrument);

    /* Scene instruments. */
    m_manager->installInstrument(new StopInstrument(Instrument::SCENE, wheelEventTypes, this));
    m_manager->installInstrument(m_wheelZoomInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::SCENE, wheelEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::SCENE, wheelEventTypes, this));

    m_manager->installInstrument(new StopInstrument(Instrument::SCENE, mouseEventTypes, this));
    m_manager->installInstrument(sceneClickInstrument);
    m_manager->installInstrument(new StopAcceptedInstrument(Instrument::SCENE, mouseEventTypes, this));
    m_manager->installInstrument(new ForwardingInstrument(Instrument::SCENE, mouseEventTypes, this));

    /* View/viewport instruments. */
    m_manager->installInstrument(m_uiElementsInstrument, InstallationMode::INSTALL_BEFORE, m_display->paintForwardingInstrument());
    m_manager->installInstrument(m_rotationInstrument, InstallationMode::INSTALL_AFTER, m_display->transformationListenerInstrument());
    m_manager->installInstrument(m_resizingInstrument);
    m_manager->installInstrument(m_dragInstrument);
    m_manager->installInstrument(m_rubberBandInstrument);
    m_manager->installInstrument(m_handScrollInstrument);
    m_manager->installInstrument(m_archiveDropInstrument);

    connect(itemClickInstrument,        SIGNAL(clicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)),       this,                           SLOT(at_item_leftClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(itemClickInstrument,        SIGNAL(doubleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)), this,                           SLOT(at_item_doubleClicked(QGraphicsView *, QGraphicsItem *, const ClickInfo &)));
    connect(sceneClickInstrument,       SIGNAL(clicked(QGraphicsView *, const ClickInfo &)),                        this,                           SLOT(at_scene_leftClicked(QGraphicsView *, const ClickInfo &)));
    connect(sceneClickInstrument,       SIGNAL(doubleClicked(QGraphicsView *, const ClickInfo &)),                  this,                           SLOT(at_scene_doubleClicked(QGraphicsView *, const ClickInfo &)));
    connect(m_dragInstrument,           SIGNAL(dragStarted(QGraphicsView *, QList<QGraphicsItem *>)),               this,                           SLOT(at_dragStarted(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_dragInstrument,           SIGNAL(dragFinished(QGraphicsView *, QList<QGraphicsItem *>)),              this,                           SLOT(at_dragFinished(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(m_resizingInstrument,       SIGNAL(resizingStarted(QGraphicsView *, QGraphicsWidget *)),                this,                           SLOT(at_resizingStarted(QGraphicsView *, QGraphicsWidget *)));
    connect(m_resizingInstrument,       SIGNAL(resizingFinished(QGraphicsView *, QGraphicsWidget *)),               this,                           SLOT(at_resizingFinished(QGraphicsView *, QGraphicsWidget *)));
    connect(m_rotationInstrument,       SIGNAL(rotationStarted(QGraphicsView *, QnResourceWidget *)),               this,                           SLOT(at_rotationStarted(QGraphicsView *, QnResourceWidget *)));
    connect(m_rotationInstrument,       SIGNAL(rotationFinished(QGraphicsView *, QnResourceWidget *)),              this,                           SLOT(at_rotationFinished(QGraphicsView *, QnResourceWidget *)));
    connect(itemContextMenuInstrument,  SIGNAL(activated(QGraphicsItem *, QEvent *)),                               this,                           SLOT(at_item_contextMenuRequested(QGraphicsItem *, QEvent *)));

    connect(m_handScrollInstrument,     SIGNAL(scrollStarted(QGraphicsView *)),                                     boundingInstrument,             SLOT(dontEnforcePosition(QGraphicsView *)));
    connect(m_handScrollInstrument,     SIGNAL(scrollFinished(QGraphicsView *)),                                    boundingInstrument,             SLOT(enforcePosition(QGraphicsView *)));

    connect(m_display,                  SIGNAL(viewportGrabbed()),                                                  m_handScrollInstrument,         SLOT(recursiveDisable()));
    connect(m_display,                  SIGNAL(viewportUngrabbed()),                                                m_handScrollInstrument,         SLOT(recursiveEnable()));
    connect(m_display,                  SIGNAL(viewportGrabbed()),                                                  m_wheelZoomInstrument,          SLOT(recursiveDisable()));
    connect(m_display,                  SIGNAL(viewportUngrabbed()),                                                m_wheelZoomInstrument,          SLOT(recursiveEnable()));

    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *)),         itemMouseForwardingInstrument,  SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *)),        itemMouseForwardingInstrument,  SLOT(recursiveEnable()));
    connect(m_dragInstrument,           SIGNAL(dragProcessStarted(QGraphicsView *)),                                itemMouseForwardingInstrument,  SLOT(recursiveDisable()));
    connect(m_dragInstrument,           SIGNAL(dragProcessFinished(QGraphicsView *)),                               itemMouseForwardingInstrument,  SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QnResourceWidget *)),        itemMouseForwardingInstrument,  SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QnResourceWidget *)),       itemMouseForwardingInstrument,  SLOT(recursiveEnable()));
    connect(m_handScrollInstrument,     SIGNAL(scrollProcessStarted(QGraphicsView *)),                              itemMouseForwardingInstrument,  SLOT(recursiveDisable()));
    connect(m_handScrollInstrument,     SIGNAL(scrollProcessFinished(QGraphicsView *)),                             itemMouseForwardingInstrument,  SLOT(recursiveEnable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandProcessStarted(QGraphicsView *)),                          itemMouseForwardingInstrument,  SLOT(recursiveDisable()));
    connect(m_rubberBandInstrument,     SIGNAL(rubberBandProcessFinished(QGraphicsView *)),                         itemMouseForwardingInstrument,  SLOT(recursiveEnable()));

    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *)),         m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *)),        m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessStarted(QGraphicsView *, QGraphicsWidget *)),         m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_resizingInstrument,       SIGNAL(resizingProcessFinished(QGraphicsView *, QGraphicsWidget *)),        m_rubberBandInstrument,         SLOT(recursiveEnable()));

    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QnResourceWidget *)),        m_dragInstrument,               SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QnResourceWidget *)),       m_dragInstrument,               SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QnResourceWidget *)),        m_rubberBandInstrument,         SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QnResourceWidget *)),       m_rubberBandInstrument,         SLOT(recursiveEnable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessStarted(QGraphicsView *, QnResourceWidget *)),        m_resizingInstrument,           SLOT(recursiveDisable()));
    connect(m_rotationInstrument,       SIGNAL(rotationProcessFinished(QGraphicsView *, QnResourceWidget *)),       m_resizingInstrument,           SLOT(recursiveEnable()));

    /* Create controls. */
    QGraphicsWidget *controlsWidget = m_uiElementsInstrument->widget();
    m_display->setLayer(controlsWidget, QnWorkbenchDisplay::UI_ELEMENTS_LAYER);

    QGraphicsLinearLayout *verticalLayout = new QGraphicsLinearLayout(Qt::Vertical);
    verticalLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
    controlsWidget->setLayout(verticalLayout);

    m_navigationItem = new NavigationItem(controlsWidget);

    verticalLayout->addStretch(0x1000);

    verticalLayout->addItem(m_navigationItem);

    connect(m_navigationItem,           SIGNAL(geometryChanged()),                                                  this,                           SLOT(at_navigationItem_geometryChanged()));

    /* Connect to display. */
    connect(m_display,                  SIGNAL(widgetChanged(QnWorkbench::ItemRole)),                               this,                           SLOT(at_display_widgetChanged(QnWorkbench::ItemRole)));
    connect(m_display,                  SIGNAL(viewportGrabbed()),                                                  this,                           SLOT(at_viewportGrabbed()));
    connect(m_display,                  SIGNAL(viewportUngrabbed()),                                                this,                           SLOT(at_viewportUngrabbed()));
    connect(m_display,                  SIGNAL(widgetAdded(QnResourceWidget *)),                                    this,                           SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(m_display,                  SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)),                         this,                           SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));

#if 0
    /* Set up context menu. */
    QAction *addFilesAction             = newAction(tr("Add file(s)"),          tr("Ins"),          this);
    QAction *addFolderAction            = newAction(tr("Add folder"),           tr("Shift+Ins"),    this);
    QAction *addCameraAction            = newAction(tr("Add camera"),           tr("Ctrl+Ins"),     this);
    QAction *newCameraAction            = newAction(tr("New camera"),           tr("Ctrl+C"),       this);
    QAction *newLayoutAction            = newAction(tr("New layout"),           tr("Ctrl+L"),       this);
    QAction *undoAction                 = newAction(tr("Undo"),                 tr("Ctrl+Z"),       this);
    QAction *redoAction                 = newAction(tr("Redo"),                 tr("Ctrl+Shift+Z"), this);
    QAction *fitInViewAction            = newAction(tr("Fit in view"),          tr("Ctrl+V"),       this);
    QAction *fullscreenAction           = newAction(tr("Toggle fullscreen"),    tr("Ctrl+Enter"),   this);
    QAction *startStopRecordingAction   = newAction(tr("Start/stop"),           tr("Alt+R"),        this);
    QAction *recordingSettingsAction    = newAction(tr("Settings"),             QString(),          this);
    QAction *saveLayoutAction           = newAction(tr("Save layout"),          tr("Ctrl+S"),       this);
    QAction *saveLayoutAsAction         = newAction(tr("Save layout as..."),    tr("Ctrl+Shift+S"), this);
    QAction *preferencesAction          = newAction(tr("Preferences"),          tr("Ctrl+P"),       this);
    QAction *exportLayoutAction         = newAction(tr("Export layout"),        tr("Ctrl+Shift+E"), this);
#endif
    //QAction *exitAction                 = newAction(tr("Exit"),                 tr("Alt+F4"),       this);
    QAction *showMotionAction           = newAction(tr("Show motion"),          tr(""),             this);
    QAction *hideMotionAction           = newAction(tr("Hide motion"),          tr(""),             this);
    
    connect(showMotionAction,           SIGNAL(triggered(bool)),                                                    this,                           SLOT(at_showMotionAction_triggered()));
    connect(hideMotionAction,           SIGNAL(triggered(bool)),                                                    this,                           SLOT(at_hideMotionAction_triggered()));

    m_contextMenu = new QMenu();
    m_contextMenu->addAction(showMotionAction);
    m_contextMenu->addAction(hideMotionAction);
    //m_contextMenu->addSeparator();
    //m_contextMenu->addAction(exitAction);
}

QnWorkbenchController::~QnWorkbenchController() {
    return;
}

QnWorkbenchDisplay *QnWorkbenchController::display() const {
    return m_display;
}

QnWorkbench *QnWorkbenchController::workbench() const {
    return m_display->workbench();
}

QnWorkbenchLayout *QnWorkbenchController::layout() const {
    return m_display->workbench()->layout();
}

QnWorkbenchGridMapper *QnWorkbenchController::mapper() const {
    return m_display->workbench()->mapper();
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
        /* Place already taken, pick closest one. Use AR-based metric. */
        QnAspectRatioGridWalker walker(aspectRatio(display()->view()->viewport()->size()) / aspectRatio(workbench()->mapper()->step()));
        QRect newGeometry = layout()->closestFreeSlot(geometry.topLeft(), geometry.size(), &walker);
        layout()->pinItem(item, newGeometry);
    }

    display()->fitInView();
}

void QnWorkbenchController::updateGeometryDelta(QnResourceWidget *widget) {
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

    m_display->bringToFront(item);
}

void QnWorkbenchController::at_resizingFinished(QGraphicsView *, QGraphicsWidget *item) {
    qDebug("RESIZING FINISHED");

    QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item);
    if(widget == NULL)
        return;

    QRect newRect = mapper()->mapToGrid(widget->geometry());
    QSet<QnWorkbenchItem *> entities = layout()->items(newRect);
    entities.remove(widget->item());
    if (entities.empty()) {
        widget->item()->setGeometry(newRect);
        updateGeometryDelta(widget);
    }

    m_display->synchronize(widget->item());
    m_display->fitInView();
}

void QnWorkbenchController::at_dragStarted(QGraphicsView *, const QList<QGraphicsItem *> &items) {
    qDebug("DRAGGING STARTED");

    /* Bring to front preserving relative order. */
    m_display->bringToFront(items);
    m_display->setLayer(items, QnWorkbenchDisplay::FRONT_LAYER);

    /* Un-raise if raised is among the dragged. */
    //QnResourceWidget *raisedWidget = display()->widget(workbench()->raisedItem());
    //if(raisedWidget != NULL && items.contains(raisedWidget))
    //    workbench()->setRaisedItem(NULL);
}

void QnWorkbenchController::at_dragFinished(QGraphicsView *view, const QList<QGraphicsItem *> &items) {
    qDebug("DRAGGING FINISHED");

    /* Get workbench items and drag delta. */
    QList<QnResourceWidget *> widgets;
    QList<QnWorkbenchItem *> workbenchItems;
    QPoint delta;
    foreach (QGraphicsItem *item, items) {
        QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item);
        if(widget == NULL)
            continue;

        if(workbenchItems.empty())
            delta = mapper()->mapToGrid(widget->geometry()).topLeft() - widget->item()->geometry().topLeft();

        widgets.push_back(widget);
        workbenchItems.push_back(widget->item());
    }
    if(workbenchItems.empty())
        return;

    bool success = false;
    if(delta.isNull()) {
        success = true;
    } else {
        QList<QRect> geometries;
        bool finished = false;

        /* Handle single widget case. */
        if(workbenchItems.size() == 1) {
            QnWorkbenchItem *draggedModel = workbenchItems[0];

            /* Find item that dragged item was dropped on. */
            QPoint cursorPos = QCursor::pos();
            QnWorkbenchItem *replacedModel = layout()->item(mapper()->mapToGrid(view->mapToScene(view->mapFromGlobal(cursorPos))));

            /* Switch places if dropping smaller one on a bigger one. */
            if(replacedModel != NULL && replacedModel != draggedModel && draggedModel->isPinned()) {
                QSizeF draggedSize = draggedModel->geometry().size();
                QSizeF replacedSize = replacedModel->geometry().size();
                if(replacedSize.width() >= draggedSize.width() && replacedSize.height() >= draggedSize.height()) {
                    workbenchItems.push_back(replacedModel);
                    geometries.push_back(replacedModel->geometry());
                    geometries.push_back(draggedModel->geometry());
                    finished = true;
                }
            }
        }

        /* Handle all other cases. */
        if(!finished) {
            QList<QRect> replacedGeometries;
            foreach (QnWorkbenchItem *model, workbenchItems) {
                QRect geometry = model->geometry().adjusted(delta.x(), delta.y(), delta.x(), delta.y());
                geometries.push_back(geometry);
                if(model->isPinned())
                    replacedGeometries.push_back(geometry);
            }

            QList<QnWorkbenchItem *> replacedModels = layout()->items(replacedGeometries).subtract(workbenchItems.toSet()).toList();
            replacedGeometries.clear();
            foreach (QnWorkbenchItem *model, replacedModels)
                replacedGeometries.push_back(model->geometry().adjusted(-delta.x(), -delta.y(), -delta.x(), -delta.y()));

            workbenchItems.append(replacedModels);
            geometries.append(replacedGeometries);
            finished = true;
        }

        success = layout()->moveItems(workbenchItems, geometries);
    }

    /* Adjust geometry deltas if everything went fine. */
    if(success)
        foreach(QnResourceWidget *widget, widgets)
            updateGeometryDelta(widget);

    /* Re-sync everything. */
    foreach(QnWorkbenchItem *model, workbenchItems)
        m_display->synchronize(model);

#if 0
    /* Deselect items that were dragged. */
    if(!delta.isNull())
        foreach(QnResourceWidget *widget, widgets)
            widget->setSelected(false);
#endif
}

void QnWorkbenchController::at_rotationStarted(QGraphicsView *, QnResourceWidget *widget) {
    m_display->bringToFront(widget);
}

void QnWorkbenchController::at_rotationFinished(QGraphicsView *, QnResourceWidget *widget) {
    if(widget == NULL)
        return; /* We may get NULL if the widget being rotated gets deleted. */

    widget->item()->setRotation(widget->rotation());
}

void QnWorkbenchController::at_item_leftClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &info) {
    if(info.modifiers() != 0)
        return;

    QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item);
    if(widget == NULL)
        return;

    QnWorkbenchItem *workbenchItem = widget->item();

    workbench()->setItem(QnWorkbench::RAISED, workbench()->item(QnWorkbench::RAISED) == workbenchItem ? NULL : workbenchItem);
    workbench()->setItem(QnWorkbench::FOCUSED, workbenchItem);
}

void QnWorkbenchController::at_item_doubleClicked(QGraphicsView *, QGraphicsItem *item, const ClickInfo &) {
    QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item);
    if(widget == NULL)
        return;

    QnWorkbenchItem *workbenchItem = widget->item();
    QnWorkbenchItem *zoomedItem = workbench()->item(QnWorkbench::ZOOMED);
    if(zoomedItem == workbenchItem) {
        QRectF viewportGeometry = m_display->viewportGeometry();
        QRectF zoomedItemGeometry = m_display->itemGeometry(zoomedItem);

        if(contains(zoomedItemGeometry.size(), viewportGeometry.size()) && !qFuzzyCompare(viewportGeometry, zoomedItemGeometry)) {
            workbench()->setItem(QnWorkbench::ZOOMED, NULL);
            workbench()->setItem(QnWorkbench::ZOOMED, workbenchItem);
        } else {
            workbench()->setItem(QnWorkbench::ZOOMED, NULL);
            workbench()->setItem(QnWorkbench::RAISED, NULL);
        }
    } else {
        workbench()->setItem(QnWorkbench::ZOOMED, workbenchItem);
    }
}

void QnWorkbenchController::at_scene_leftClicked(QGraphicsView *, const ClickInfo &) {
    if(workbench() == NULL)
        return;

    workbench()->setItem(QnWorkbench::RAISED, NULL);
}

void QnWorkbenchController::at_scene_doubleClicked(QGraphicsView *, const ClickInfo &) {
    if(workbench() == NULL)
        return;

    workbench()->setItem(QnWorkbench::ZOOMED, NULL);
    m_display->fitInView();
}

void QnWorkbenchController::at_viewportGrabbed() {
    qDebug("VIEWPORT GRABBED");
}

void QnWorkbenchController::at_viewportUngrabbed() {
    qDebug("VIEWPORT UNGRABBED");
}

void QnWorkbenchController::at_display_widgetChanged(QnWorkbench::ItemRole role) {
    QnResourceWidget *widget = m_display->widget(role);
    QnResourceWidget *oldWidget = m_widgetByRole[role];
    if(widget == oldWidget)
        return;

    m_widgetByRole[role] = widget;

    switch(role) {
    case QnWorkbench::FOCUSED:
        /* Update navigation item's target. */
        m_navigationItem->setVideoCamera(widget == NULL ? NULL : widget->display()->camera());
        break;
    case QnWorkbench::ZOOMED: {
        bool effective = widget == NULL;
        m_resizingInstrument->setEffective(effective);
        m_resizingInstrument->resizeHoverInstrument()->setEffective(effective);
        m_dragInstrument->setEffective(effective);
        break;
    }
    default:
        break;
    }
}

void QnWorkbenchController::at_display_widgetAdded(QnResourceWidget *widget) {
    if(widget->display() == NULL || widget->display()->camera() == NULL)
        return;

    QnSequrityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSequrityCamResource>();
    if(cameraResource != NULL)
        m_navigationItem->addReserveCamera(widget->display()->camera());
}

void QnWorkbenchController::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(widget->display() == NULL || widget->display()->camera() == NULL)
        return;

    QnSequrityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSequrityCamResource>();
    if(cameraResource != NULL)
        m_navigationItem->removeReserveCamera(widget->display()->camera());
}

void QnWorkbenchController::at_navigationItem_geometryChanged() {
    m_display->setViewportMargins(QMargins(
        0,
        0,
        0,
        std::floor(m_navigationItem->size().height())
    ));
}

void QnWorkbenchController::at_item_contextMenuRequested(QGraphicsItem *item, QEvent *event) {
    assert(event->type() == QEvent::GraphicsSceneContextMenu);

    QGraphicsSceneContextMenuEvent *e = static_cast<QGraphicsSceneContextMenuEvent *>(event);

    m_contextMenu->exec(e->screenPos());
}

void QnWorkbenchController::at_showMotionAction_triggered() {
    displayMotionGrid(display()->scene()->selectedItems(), true);
}

void QnWorkbenchController::at_hideMotionAction_triggered() {
    displayMotionGrid(display()->scene()->selectedItems(), false);
}

void QnWorkbenchController::displayMotionGrid(const QList<QGraphicsItem *> &items, bool display) {
    foreach(QGraphicsItem *item, items) {
        QnResourceWidget *widget = dynamic_cast<QnResourceWidget *>(item);
        if(widget == NULL)
            continue;

        widget->setMotionGridDisplayed(display);
    }
}

