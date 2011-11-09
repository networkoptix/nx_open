#include "scenecontroller.h"
#include "scenecontroller_p.h"
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
#include <ui/widgets2/centralwidget.h>
#include <ui/widgets2/animatedwidget.h>
#include <ui/widgets2/layoutitem.h>
#include <ui/widgets2/celllayout.h>
#include <ui/view/viewport_animator.h>

namespace {
    struct LayoutDescription {
        const char *pattern;
        const char *fileName;
    } layouts[] = {
        {
            "4000"
            "0000"
            "0000"
            "0000",
            ":/images/1.bmp"
        },
        {
            "2020"
            "0000"
            "2020"
            "0000",
            ":/images/1-1-1-1.bmp"
        },
        {
            "2020"
            "0000"
            "2011"
            "0011",
            ":/images/1-1-1-4.bmp"
        },
        {
            "2011"
            "0011"
            "2011"
            "0011",
            ":/images/1-4-1-4.bmp"
        },
        {
            "2011"
            "0011"
            "1111"
            "1111",
            ":/images/1-4-4-4.bmp"
        },
        {
            "3001"
            "0001"
            "0001"
            "1111",
            ":/images/1x3+.bmp"
        },
        {
            "1111"
            "1201"
            "1001"
            "1111",
            "1"
        },
        {
            "2020"
            "0000"
            "1201"
            "1001",
            "2"
        },
        {
            "1111"
            "1111"
            "1111"
            "1111",
            ":/images/4-4-4-4.png"
        },
        {
            NULL,
            NULL
        }
    };

    void calculateExpansionValues(qreal start, qreal end, qreal center, qreal newLength, qreal *deltaStart, qreal *deltaEnd) {
        qreal newStart = center - newLength / 2;
        qreal newEnd = center + newLength / 2;

        if(newStart > start) {
            newEnd += start - newStart;
            newStart = start;
        }

        if(newEnd < end) {
            newStart += end - newEnd;
            newEnd = end;
        }

        *deltaStart = newStart - start;
        *deltaEnd = newEnd - end;
    }

    const qreal spacing = 25;

    /** Size multiplier for focused widgets. */
    const qreal focusExpansion = 1.5;

    /** Maximal expanded size of a focused widget, relative to viewport size. */
    const qreal maxExpandedSize = 0.8;

    /** Lower size boundary. */
    const QSizeF lowerSizeBound = QSizeF(1.0, 1.0);

    const qreal normalZ = 0.0;
    const qreal focusedZ = 1.0;

    const int zoomAnimationDurationMsec = 250;

} // anonymous namespace

void SceneControllerPrivate::toggleFocus(AnimatedWidget *widget) {
    if(widget == focusedWidget) {
        if(focusedWidget != NULL) {
            if(focusedExpanded) {
                focusedContract();
            } else {
                focusedExpand();
            }
        }
    } else {
        if(focusedWidget != NULL) {
            if(focusedExpanded)
                focusedContract();

            if(focusedZoomed)
                focusedUnzoom();

            focusedWidget->setZValue(normalZ);
        }

        focusedWidget = widget;
        
        if(focusedWidget != NULL) {
            focusedWidget->setZValue(focusedZ);

            focusedExpand();
        }
    }
}

void SceneControllerPrivate::toggleZoom(AnimatedWidget *widget) {
    if(widget == focusedWidget) {
        if(focusedWidget != NULL) {
            if(focusedExpanded)
                focusedContract();

            if(focusedZoomed) {
                focusedUnzoom();
            } else {
                focusedZoom();
            }
        }
    } else {
        if(focusedWidget != NULL) {
            if(focusedExpanded)
                focusedContract();

            if(focusedZoomed)
                focusedUnzoom();

            focusedWidget->setZValue(normalZ);
        }

        focusedWidget = widget;

        if(focusedWidget != NULL) {
            focusedWidget->setZValue(focusedZ);

            focusedZoom();
        }
    }
}

void SceneControllerPrivate::focusedExpand() {
    if(focusedWidget == NULL || focusedExpanded)
        return;

    QRectF viewportRect = QnSceneUtility::mapRectToScene(view, view->viewport()->rect());
    QRectF widgetRect = centralLayout->mapFromGrid(centralLayout->rect(focusedWidget));

    QPointF viewportCenter = viewportRect.center();
    QPointF widgetCenter = widgetRect.center();

    QSizeF newWidgetSize = widgetRect.size() * focusExpansion;
    QSizeF maxWidgetSize = viewportRect.size() * maxExpandedSize;

    /* Allow expansion no further than the maximal size, but no less than current size. */
    newWidgetSize =  QnSceneUtility::bounded(newWidgetSize, maxWidgetSize,     Qt::KeepAspectRatio);
    newWidgetSize = QnSceneUtility::expanded(newWidgetSize, widgetRect.size(), Qt::KeepAspectRatio);

    /* Calculate expansion values. Expand towards the screen center. */
    qreal xp1 = 0.0, xp2 = 0.0, yp1 = 0.0, yp2 = 0.0;
    calculateExpansionValues(widgetRect.left(), widgetRect.right(),  viewportCenter.x(), newWidgetSize.width(),  &xp1, &xp2);
    calculateExpansionValues(widgetRect.top(),  widgetRect.bottom(), viewportCenter.y(), newWidgetSize.height(), &yp1, &yp2);
    QRectF newWidgetRect = widgetRect.adjusted(xp1, yp1, xp2, yp2);

    focusedWidget->setGeometry(newWidgetRect);
    
    focusedExpanded = true;
}

void SceneControllerPrivate::focusedContract() {
    if(focusedWidget == NULL || !focusedExpanded)
        return;

    QRectF rect = centralLayout->mapFromGrid(centralLayout->rect(focusedWidget));
    focusedWidget->setGeometry(rect);

    focusedExpanded = false;
}

void SceneControllerPrivate::focusedZoom() {
    if(focusedWidget == NULL || focusedZoomed)
        return;

    if(viewportAnimator->isAnimating()) {
        viewportAnimator->reverse();
    } else {
        viewportAnimator->moveTo(focusedWidgetRect(), zoomAnimationDurationMsec);
    }

    focusedZoomed = true;
}

void SceneControllerPrivate::focusedUnzoom() {
    if(focusedWidget == NULL || !focusedZoomed)
        return;

    if(viewportAnimator->isAnimating()) {
        viewportAnimator->reverse();
    } else {
        viewportAnimator->moveTo(fitInViewRect(), zoomAnimationDurationMsec);
    }

    focusedZoomed = false;
}

void SceneControllerPrivate::updateBoundingRects() {
    QRectF boundingRect = focusedZoomed ? focusedWidgetRect() : fitInViewRect();

    boundingInstrument->setPositionBounds(view, boundingRect, 0.0);
    boundingInstrument->setSizeBounds(view, lowerSizeBound, Qt::KeepAspectRatioByExpanding, boundingRect.size(), Qt::KeepAspectRatioByExpanding);
}

void SceneControllerPrivate::toggleFocusedFitting() {
    assert(focusedWidget != NULL && focusedZoomed);

    if(viewportAnimator->isAnimating()) {
        viewportAnimator->reverse();
    } else {
        viewportAnimator->moveTo(focusedWidgetRect(), zoomAnimationDurationMsec);
    }
}

void SceneControllerPrivate::fitInView() {
    QRectF currentRect = viewportRect();
    QRectF targetRect = fitInViewRect();
    if(qFuzzyCompare(currentRect.center(), targetRect.center()) && qFuzzyCompare(QnSceneUtility::scaleFactor(currentRect.size(), targetRect.size(), Qt::KeepAspectRatioByExpanding), 1.0))
        return;

    if(focusedZoomed) {
        focusedUnzoom();
        return;
    }

    viewportAnimator->moveTo(fitInViewRect(), zoomAnimationDurationMsec);
}

QRectF SceneControllerPrivate::fitInViewRect() const {
    return centralWidget->geometry();
}

QRectF SceneControllerPrivate::focusedWidgetRect() const {
    return centralWidget->mapRectToScene(centralLayout->mapFromGrid(centralLayout->rect(focusedWidget)));
}

QRectF SceneControllerPrivate::viewportRect() const {
    return QnSceneUtility::mapRectToScene(view, view->viewport()->rect());
}

SceneController::SceneController(QGraphicsView *view, QGraphicsScene *scene, QObject *parent):
    QObject(parent),
    d_ptr(new SceneControllerPrivate())
{
    Q_D(SceneController);

    const_cast<SceneController *&>(d->q_ptr) = this;
    d->scene = scene;
    d->view = view;
    d->manager = new InstrumentManager(this);
    d->scene->setItemIndexMethod(QGraphicsScene::NoIndex);

    /* Install instruments. */
    d->manager->registerScene(d->scene);
    ClickInstrument *itemClickInstrument = new ClickInstrument(ClickInstrument::WATCH_ITEM, this);
    ClickInstrument *sceneClickInstrument = new ClickInstrument(ClickInstrument::WATCH_SCENE, this);
    d->dragInstrument = new DragInstrument(this);
    d->handScrollInstrument = new HandScrollInstrument(this);
    d->boundingInstrument = new BoundingInstrument(this);
    d->wheelZoomInstrument = new WheelZoomInstrument(this);

    QEvent::Type sceneEventTypes[] = {
        QEvent::GraphicsSceneMousePress,
        QEvent::GraphicsSceneMouseMove,
        QEvent::GraphicsSceneMouseRelease,
        QEvent::GraphicsSceneMouseDoubleClick,
    };

    d->manager->installInstrument(new StopInstrument(Instrument::makeSet(sceneEventTypes), Instrument::makeSet(), Instrument::makeSet(QEvent::Wheel), false, this));
    d->manager->installInstrument(sceneClickInstrument);
    d->manager->installInstrument(new StopAcceptedInstrument(Instrument::makeSet(sceneEventTypes), Instrument::makeSet(), Instrument::makeSet(), false, this));
    d->manager->installInstrument(new SelectionInstrument(this));
    d->manager->installInstrument(new ForwardingInstrument(Instrument::makeSet(sceneEventTypes), Instrument::makeSet(), Instrument::makeSet(), false, this));
    d->manager->installInstrument(new TransformListenerInstrument(this));
    d->manager->installInstrument(d->wheelZoomInstrument);
    d->manager->installInstrument(d->dragInstrument);
    d->manager->installInstrument(new RubberBandInstrument(this));
    d->manager->installInstrument(d->handScrollInstrument);
    d->manager->installInstrument(new ContextMenuInstrument(this));
    d->manager->installInstrument(itemClickInstrument);
    d->manager->installInstrument(d->boundingInstrument);

    connect(itemClickInstrument,     SIGNAL(clicked(QGraphicsView *, QGraphicsItem *)),                 this,                  SLOT(at_item_clicked(QGraphicsView *, QGraphicsItem *)));
    connect(itemClickInstrument,     SIGNAL(doubleClicked(QGraphicsView *, QGraphicsItem *)),           this,                  SLOT(at_item_doubleClicked(QGraphicsView *, QGraphicsItem *)));
    connect(sceneClickInstrument,    SIGNAL(clicked(QGraphicsView *)),                                  this,                  SLOT(at_scene_clicked(QGraphicsView *)));
    connect(sceneClickInstrument,    SIGNAL(doubleClicked(QGraphicsView *)),                            this,                  SLOT(at_scene_doubleClicked(QGraphicsView *)));
    connect(d->dragInstrument,       SIGNAL(draggingStarted(QGraphicsView *, QList<QGraphicsItem *>)),  this,                  SLOT(at_draggingStarted(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(d->dragInstrument,       SIGNAL(draggingFinished(QGraphicsView *, QList<QGraphicsItem *>)), this,                  SLOT(at_draggingFinished(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(d->handScrollInstrument, SIGNAL(scrollingStarted(QGraphicsView *)),                         d->boundingInstrument, SLOT(dontEnforcePosition(QGraphicsView *)));
    connect(d->handScrollInstrument, SIGNAL(scrollingFinished(QGraphicsView *)),                        d->boundingInstrument, SLOT(enforcePosition(QGraphicsView *)));

    /* Create viewport animator. */
    d->viewportAnimator = new QnViewportAnimator(view, this);
    d->viewportAnimator->setMovementSpeed(4.0);
    d->viewportAnimator->setScalingSpeed(32.0);
    connect(d->viewportAnimator, SIGNAL(animationStarted()),  d->boundingInstrument,   SLOT(recursiveDisable()));
    connect(d->viewportAnimator, SIGNAL(animationStarted()),  d->handScrollInstrument, SLOT(recursiveDisable()));
    connect(d->viewportAnimator, SIGNAL(animationStarted()),  d->wheelZoomInstrument,  SLOT(recursiveDisable()));
    connect(d->viewportAnimator, SIGNAL(animationFinished()), d->boundingInstrument,   SLOT(recursiveEnable()));
    connect(d->viewportAnimator, SIGNAL(animationFinished()), d->handScrollInstrument, SLOT(recursiveEnable()));
    connect(d->viewportAnimator, SIGNAL(animationFinished()), d->wheelZoomInstrument,  SLOT(recursiveEnable()));
    connect(d->viewportAnimator, SIGNAL(animationFinished()), this,                    SLOT(updateBoundingRects()));

    /* Create central widget. */
    d->centralWidget = new CentralWidget();
    d->centralWidget->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding, QSizePolicy::GroupBox));
    connect(d->centralWidget, SIGNAL(geometryChanged()), this, SLOT(at_centralWidget_geometryChanged()));
    d->scene->addItem(d->centralWidget);

    /* Create central layout. */
    d->centralLayout = new CellLayout();
    d->centralLayout->setContentsMargins(spacing, spacing, spacing, spacing);
    d->centralLayout->setSpacing(spacing);
    d->centralLayout->setCellSize(200, 150);
    d->centralWidget->setLayout(d->centralLayout);

    /* Create items. */
    for (int i = 0; i < 16; ++i) {
        AnimatedWidget *widget = new AnimatedWidget(d->centralWidget);

        widget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true); /* Optimization. */
        widget->setFlag(QGraphicsItem::ItemIsSelectable, true);
        widget->setFlag(QGraphicsItem::ItemIsFocusable, true);

        widget->setExtraFlag(GraphicsWidget::ItemIsResizable);
        widget->setInteractive(true);

        widget->setAnimationsEnabled(true);
        widget->setWindowTitle(SceneController::tr("item # %1").arg(i + 1));
        widget->setPos(QPointF(-500, -500));
        widget->setZValue(normalZ);

        QGraphicsGridLayout *layout = new QGraphicsGridLayout(widget);
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);

        GraphicsLayoutItem *item = new GraphicsLayoutItem(widget);
        layout->addItem(item, 0, 0, 1, 1, Qt::AlignCenter);
        item->setAcceptedMouseButtons(0);

        connect(widget, SIGNAL(resizingStarted()),  this, SLOT(at_widget_resizingStarted()));
        connect(widget, SIGNAL(resizingFinished()), this, SLOT(at_widget_resizingFinished()));

        connect(widget, SIGNAL(resizingStarted()),  d->dragInstrument, SLOT(stopCurrentOperation()));

        d->animatedWidgets.push_back(widget);
    }

    /* Initialize view. */
    view->setScene(d->scene);
    view->setDragMode(QGraphicsView::NoDrag);

    d->boundingInstrument->setSizeEnforced(view, true);
    d->boundingInstrument->setPositionEnforced(view, true);
    d->boundingInstrument->setScalingSpeed(view, 32.0);
    d->boundingInstrument->setMovementSpeed(view, 4.0);

#ifndef QT_NO_OPENGL
    if (QGLFormat::hasOpenGL()) {
        QGLFormat glFormat;
        glFormat.setOption(QGL::SampleBuffers); /* Multisampling. */
        glFormat.setSwapInterval(1); /* Turn vsync on. */
        view->setViewport(new QGLWidget(glFormat));
    }
#endif // QT_NO_OPENGL

    /* Turn on antialiasing at QPainter level. */
    view->setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing); 
    view->setRenderHints(QPainter::SmoothPixmapTransform);

    /* In OpenGL mode this one seems to be ignored, but it will help in software mode. */
    view->setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

    /* All our items save and restore painter state. */
    view->setOptimizationFlag(QGraphicsView::DontSavePainterState, false); /* Can be turned on if we won't be using framed widgets. */ 
    view->setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);

    /* Don't even try to uncomment this one, it slows everything down. */
    //setCacheMode(QGraphicsView::CacheBackground);

    /* We don't need scrollbars & frame. */
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setFrameShape(QFrame::NoFrame);

    /* Add actions. */
    int number = 0;
    for(LayoutDescription *desc = layouts; desc->pattern != NULL; desc++) {
        QAction *action;
        if(QFile::exists(QLatin1String(desc->fileName)))
            action = new QAction(QIcon(QLatin1String(desc->fileName)), tr(""), this);
        else
            action = new QAction(QLatin1String(desc->fileName), this);

        action->setShortcut(QKeySequence(Qt::Key_F1 + number++));
        action->setShortcutContext(Qt::WindowShortcut);
        action->setAutoRepeat(false);
        action->setProperty("rowCount", 4);
        action->setProperty("columnCount", 4);
        action->setProperty("preset", QByteArray(desc->pattern));
        view->addAction(action);

        connect(action, SIGNAL(triggered()), this, SLOT(at_relayoutAction_triggered()));
    }

    /* Add change mode action. */
    QAction *action = new QAction(tr("+_+"), this);
    view->addAction(action);
    connect(action, SIGNAL(triggered()), this, SLOT(at_changeModeAction_triggered()));

    d->manager->registerView(view);
}

SceneController::~SceneController() {
    return;
}

QGraphicsScene *SceneController::scene() const {
    return d_func()->scene;
}

QGraphicsView *SceneController::view() const {
    return d_func()->view;
}

SceneController::Mode SceneController::mode() const {
    return d_func()->mode;
}

void SceneController::setMode(Mode mode) {
    Q_D(SceneController);

    if(mode == d->mode)
        return;

    d->mode = mode;
    foreach (AnimatedWidget *widget, d->animatedWidgets)
        widget->setInteractive(mode == EDITING);
    d->centralWidget->setGridVisible(mode == EDITING);

    d->boundingInstrument->setEnabled(mode == VIEWING);
    d->dragInstrument->setEnabled(mode == EDITING);
}

void SceneController::at_relayoutAction_triggered() {
    QAction *action = qobject_cast<QAction *>(sender());
    if(action == NULL)
        return; /* Should never happen, actually. */

    relayoutItems(action->property("rowCount").toInt(), action->property("columnCount").toInt(), action->property("preset").toByteArray());
}

void SceneController::at_centralWidget_geometryChanged() {
    Q_D(SceneController);

    QRect bounds = d->centralLayout->bounds();
    if(bounds == d->lastCentralLayoutBounds)
        return;
    
    QPointF sceneDelta = d->centralLayout->mapFromGrid(QPoint(0, 0)) - d->centralLayout->mapFromGrid(d->lastCentralLayoutBounds.topLeft() - bounds.topLeft());
    d->lastCentralLayoutBounds = bounds;
    
    foreach(AnimatedWidget *animatedWidget, d->animatedWidgets) {
        animatedWidget->setAnimationsEnabled(false);
        animatedWidget->moveBy(-sceneDelta.x(), -sceneDelta.y());
        animatedWidget->setAnimationsEnabled(true);
    }
    d->centralWidget->moveBy(sceneDelta.x(), sceneDelta.y());
    d->centralLayout->invalidate();
    d->centralLayout->activate();

    d->boundingInstrument->setPositionBounds(NULL, d->centralWidget->geometry(), 0.0);
    d->boundingInstrument->setSizeBounds(NULL, QSizeF(100, 100), Qt::KeepAspectRatioByExpanding, d->centralWidget->size(), Qt::KeepAspectRatioByExpanding);
}

void SceneController::at_widget_resizingStarted() {
    qDebug("RESIZING STARTED");

    AnimatedWidget *widget = static_cast<AnimatedWidget *>(sender());
    widget->setAnimationsEnabled(false);
}

void SceneController::at_widget_resizingFinished() {
    Q_D(SceneController);

    qDebug("RESIZING FINISHED");

    AnimatedWidget *widget = static_cast<AnimatedWidget *>(sender());
    widget->setAnimationsEnabled(true);

    QRect newRect = d->centralLayout->mapToGrid(widget->geometry());
    QSet<QGraphicsLayoutItem *> items = d->centralLayout->itemsAt(newRect);
    items.remove(widget);
    if (items.empty())
        d->centralLayout->moveItem(widget, newRect);

    d->centralLayout->invalidate();
    d->centralLayout->activate();
}

void SceneController::at_draggingStarted(QGraphicsView *, QList<QGraphicsItem *> items) {
    qDebug("DRAGGING STARTED");

    foreach (QGraphicsItem *item, items)
        if (AnimatedWidget *widget = dynamic_cast<AnimatedWidget *>(item))
            widget->setAnimationsEnabled(false);
}

void SceneController::at_draggingFinished(QGraphicsView *view, QList<QGraphicsItem *> items) {
    qDebug("DRAGGING FINISHED");

    Q_D(SceneController);

    AnimatedWidget *draggedWidget = NULL;
    foreach (QGraphicsItem *item, items) {
        draggedWidget = dynamic_cast<AnimatedWidget *>(item);
        if (draggedWidget != NULL)
            draggedWidget->setAnimationsEnabled(true);
    }

    if(draggedWidget == NULL)
        return;

    CellLayout *layout = d->centralLayout;
    QPoint delta = layout->mapToGrid(draggedWidget->geometry()).topLeft() - layout->rect(draggedWidget).topLeft();

    QList<QGraphicsLayoutItem *> layoutItems;
    QList<QRect> rects;
    foreach (QGraphicsItem *item, items)
        if (QGraphicsLayoutItem *layoutItem = dynamic_cast<QGraphicsLayoutItem *>(item))
            layoutItems.push_back(layoutItem);

    QGraphicsLayoutItem *replacedItem = layout->itemAt(layout->mapToGrid(d->centralWidget->mapFromScene(view->mapToScene(view->mapFromGlobal(QCursor::pos())))));
    if (layoutItems.size() == 1 && replacedItem != NULL) {
        layoutItems.push_back(replacedItem);
        rects.push_back(layout->rect(replacedItem));
        rects.push_back(layout->rect(layoutItems[0]));
    } else {
        foreach (QGraphicsLayoutItem *layoutItem, layoutItems)
            rects.push_back(layout->rect(layoutItem).adjusted(delta.x(), delta.y(), delta.x(), delta.y()));

        QList<QGraphicsLayoutItem *> replacedItems = layout->itemsAt(rects).subtract(layoutItems.toSet()).toList();
        QList<QRect> replacedRects;
        foreach (QGraphicsLayoutItem *layoutItem, replacedItems)
            replacedRects.push_back(layout->rect(layoutItem).adjusted(-delta.x(), -delta.y(), -delta.x(), -delta.y()));

        layoutItems.append(replacedItems);
        rects.append(replacedRects);
    }

    layout->moveItems(layoutItems, rects);
    layout->invalidate();
    layout->activate();
}

void SceneController::at_item_clicked(QGraphicsView *, QGraphicsItem *item) {
    d_func()->toggleFocus(dynamic_cast<AnimatedWidget *>(item));
}

void SceneController::at_item_doubleClicked(QGraphicsView *, QGraphicsItem *item) {
    Q_D(SceneController);

    if(d->focusedZoomed) {
        QRectF viewportRect = d->viewportRect();
        QRectF focusedWidgetRect = d->focusedWidgetRect();

        if(QnSceneUtility::contains(focusedWidgetRect.size(), viewportRect.size()) && !qFuzzyCompare(viewportRect, focusedWidgetRect)) {
            d->toggleFocusedFitting();
        } else {
            d->toggleZoom(dynamic_cast<AnimatedWidget *>(item));
        }
    } else {
        d->toggleZoom(dynamic_cast<AnimatedWidget *>(item));
    }
}

void SceneController::at_scene_clicked(QGraphicsView *) {
    d_func()->toggleFocus(NULL);
}

void SceneController::at_scene_doubleClicked(QGraphicsView *) {
    d_func()->fitInView();
}

void SceneController::at_widget_destroyed() {
    d_func()->animatedWidgets.removeOne(static_cast<AnimatedWidget *>(sender()));
}

void SceneController::updateBoundingRects() {
    d_func()->updateBoundingRects();
}

void SceneController::relayoutItems(int rowCount, int columnCount, const QByteArray &preset) {
    Q_D(SceneController);

    QByteArray localPreset = preset.size() == rowCount * columnCount ? preset : QByteArray(rowCount * columnCount, '1');

    while(d->centralLayout->count() != 0)
        d->centralLayout->removeAt(0);

    int i = 0;
    for (int row = 0; row < rowCount; ++row) {
        for (int column = 0; column < columnCount; ++column) {
            if (QGraphicsWidget *widget = d->animatedWidgets.value(i, 0)) {
                const int span = localPreset.at(row * columnCount + column) - '0';
                if (span > 0) {
                    d->centralLayout->addItem(widget, row, column, span, span, Qt::AlignCenter);
                    ++i;
                }
            }
        }
    }
    
    while (i < d->animatedWidgets.size()) {
        QGraphicsWidget *widget = d->animatedWidgets.at(i++);
        widget->setGeometry(QRectF(QPointF(-500, -500), widget->preferredSize()));
    }
}

void SceneController::at_changeModeAction_triggered() {
    setMode(mode() == EDITING ? VIEWING : EDITING);
}
