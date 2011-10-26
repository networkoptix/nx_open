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
#include <instruments/instrumentmanager.h>
#include <instruments/handscrollinstrument.h>
#include <instruments/wheelzoominstrument.h>
#include <instruments/rubberbandinstrument.h>
#include <instruments/draginstrument.h>
#include <instruments/contextmenuinstrument.h>
#include <instruments/clickinstrument.h>
#include <instruments/boundinginstrument.h>
#include <instruments/sceneinputstopinstrument.h>
#include <instruments/sceneinputforwardinginstrument.h>
#include <widgets/centralwidget.h>
#include <widgets/animatedwidget.h>
#include <widgets/layoutitem.h>
#include <widgets/celllayout.h>
#include "setteranimation.h"

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

    class ViewportPositionSetter {
    public:
        ViewportPositionSetter(QGraphicsView *view): m_view(view) {}

        void operator()(const QVariant &value) const {
            InstrumentUtility::moveViewportTo(m_view, value.toPointF());
        }

    private:
        QGraphicsView *m_view;
    };

    class ViewportScaleSetter {
    public:
        ViewportScaleSetter(QGraphicsView *view, InstrumentUtility::BoundingMode mode): m_view(view), m_mode(mode) {}

        void operator()(const QVariant &value) const {
            InstrumentUtility::scaleViewportTo(m_view, value.toSizeF(), m_mode);
        }

    private:
        QGraphicsView *m_view;
        InstrumentUtility::BoundingMode m_mode;
    };


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

    QRectF viewportRect = InstrumentUtility::mapRectToScene(view, view->viewport()->rect());
    QRectF widgetRect = centralLayout->mapFromGrid(centralLayout->rect(focusedWidget));

    QPointF viewportCenter = viewportRect.center();
    QPointF widgetCenter = widgetRect.center();

    QSizeF newWidgetSize = widgetRect.size() * focusExpansion;
    QSizeF maxWidgetSize = viewportRect.size() * maxExpandedSize;

    /* Allow expansion no further than the maximal size, but no less than current size. */
    newWidgetSize =  InstrumentUtility::bounded(newWidgetSize, maxWidgetSize,     Qt::KeepAspectRatio);
    newWidgetSize = InstrumentUtility::expanded(newWidgetSize, widgetRect.size(), Qt::KeepAspectRatio);

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

void SceneControllerPrivate::initZoomAnimation(const QRectF &unzoomed, const QRectF &zoomed) {
    scaleAnimation->setStartValue(unzoomed.size());
    scaleAnimation->setEndValue(zoomed.size());

    positionAnimation->setStartValue(unzoomed.center());
    positionAnimation->setEndValue(zoomed.center());
}

void SceneControllerPrivate::animateZoomToFocused() {
    initZoomAnimation(
        viewportRect(),
        focusedWidgetRect()
    );

    boundingInstrument->disable();
    handScrollInstrument->disable();
    //wheelZoomInstrument->disable();

    zoomAnimation->setCurrentTime(0);
    zoomAnimation->setDirection(QAbstractAnimation::Forward);
    zoomAnimation->start();
}

void SceneControllerPrivate::animateZoomToGlobal() {
    initZoomAnimation(
        fitInViewRect(),
        viewportRect()
    );

    boundingInstrument->disable();
    handScrollInstrument->disable();
    //wheelZoomInstrument->disable();

    zoomAnimation->setCurrentTime(zoomAnimation->duration());
    zoomAnimation->setDirection(QAbstractAnimation::Backward);
    zoomAnimation->start();
}

void SceneControllerPrivate::zoomAnimationFinished() {
    boundingInstrument->enable();
    handScrollInstrument->enable();
    //wheelZoomInstrument->enable();

    QRectF boundingRect = focusedZoomed ? focusedWidgetRect() : fitInViewRect();

    boundingInstrument->setPositionBounds(view, boundingRect, 0.0);
    boundingInstrument->setSizeBounds(view, lowerSizeBound, InstrumentUtility::OutBound, boundingRect.size(), InstrumentUtility::OutBound);
}

void SceneControllerPrivate::focusedZoom() {
    if(focusedWidget == NULL || focusedZoomed)
        return;

    if(zoomAnimation->state() == QAbstractAnimation::Running) {
        zoomAnimation->setDirection(QAbstractAnimation::Forward);
    } else {
        animateZoomToFocused();
    }

    focusedZoomed = true;
}

void SceneControllerPrivate::focusedUnzoom() {
    if(focusedWidget == NULL || !focusedZoomed)
        return;

    if(zoomAnimation->state() == QAbstractAnimation::Running) {
        zoomAnimation->setDirection(QAbstractAnimation::Backward);
    } else {
        animateZoomToGlobal();
    }

    focusedZoomed = false;
}

void SceneControllerPrivate::toggleFocusedFitting() {
    assert(focusedWidget != NULL && focusedZoomed);

    if(zoomAnimation->state() == QAbstractAnimation::Running) {
        zoomAnimation->setDirection(zoomAnimation->direction() == QAbstractAnimation::Forward ? QAbstractAnimation::Backward : QAbstractAnimation::Forward);
    } else {
        animateZoomToFocused();
    }
}

void SceneControllerPrivate::fitInView() {
    QRectF currentRect = viewportRect();
    QRectF targetRect = fitInViewRect();
    if(qFuzzyCompare(currentRect.center(), targetRect.center()) && qFuzzyCompare(InstrumentUtility::calculateScale(currentRect.size(), targetRect.size(), InstrumentUtility::OutBound), 1.0))
        return;

    if(focusedZoomed) {
        focusedUnzoom();
        return;
    }

    if(zoomAnimation->state() == QAbstractAnimation::Running)
        return;

    animateZoomToGlobal();
}

QRectF SceneControllerPrivate::fitInViewRect() const {
    return centralWidget->geometry();
}

QRectF SceneControllerPrivate::focusedWidgetRect() const {
    return centralWidget->mapRectToScene(centralLayout->mapFromGrid(centralLayout->rect(focusedWidget)));
}

QRectF SceneControllerPrivate::viewportRect() const {
    return InstrumentUtility::mapRectToScene(view, view->viewport()->rect());
}

SceneController::SceneController(QGraphicsView *view, QObject *parent):
    QObject(parent),
    d_ptr(new SceneControllerPrivate())
{
    Q_D(SceneController);

    const_cast<SceneController *&>(d->q_ptr) = this;
    d->scene = new QGraphicsScene(this);
    d->manager = new InstrumentManager(this);
    
    d->zoomAnimation = new QParallelAnimationGroup(this);
    d->scaleAnimation = new SetterAnimation(this);
    d->positionAnimation = new SetterAnimation(this);
    d->zoomAnimation->addAnimation(d->scaleAnimation);
    d->zoomAnimation->addAnimation(d->positionAnimation);
    d->scaleAnimation->setSetter(ViewportScaleSetter(view, InstrumentUtility::OutBound));
    d->positionAnimation->setSetter(ViewportPositionSetter(view));
    d->scaleAnimation->setDuration(zoomAnimationDurationMsec);
    d->positionAnimation->setDuration(zoomAnimationDurationMsec);
    connect(d->zoomAnimation, SIGNAL(finished()), this, SLOT(at_zoomAnimation_finished()));

    d->scene->setItemIndexMethod(QGraphicsScene::NoIndex);

    /* Install instruments. */
    d->manager->registerScene(d->scene);
    ClickInstrument *itemClickInstrument = new ClickInstrument(ClickInstrument::WATCH_ITEM, this);
    ClickInstrument *sceneClickInstrument = new ClickInstrument(ClickInstrument::WATCH_SCENE, this);
    DragInstrument *dragInstrument = new DragInstrument(this);
    d->handScrollInstrument = new HandScrollInstrument(this);
    d->boundingInstrument = new BoundingInstrument(this);
    d->wheelZoomInstrument = new WheelZoomInstrument(this);

    d->manager->installInstrument(new SceneInputStopInstrument(this));
    d->manager->installInstrument(sceneClickInstrument);
    d->manager->installInstrument(new SceneInputForwardingInstrument(this));
    d->manager->installInstrument(d->wheelZoomInstrument);
    d->manager->installInstrument(dragInstrument);
    d->manager->installInstrument(new RubberBandInstrument(this));
    d->manager->installInstrument(d->handScrollInstrument);
    d->manager->installInstrument(new ContextMenuInstrument(this));
    d->manager->installInstrument(itemClickInstrument);
    d->manager->installInstrument(d->boundingInstrument);

    connect(itemClickInstrument,     SIGNAL(clicked(QGraphicsView *, QGraphicsItem *)),                 this,                  SLOT(at_item_clicked(QGraphicsView *, QGraphicsItem *)));
    connect(itemClickInstrument,     SIGNAL(doubleClicked(QGraphicsView *, QGraphicsItem *)),           this,                  SLOT(at_item_doubleClicked(QGraphicsView *, QGraphicsItem *)));
    connect(sceneClickInstrument,    SIGNAL(clicked(QGraphicsView *)),                                  this,                  SLOT(at_scene_clicked(QGraphicsView *)));
    connect(sceneClickInstrument,    SIGNAL(doubleClicked(QGraphicsView *)),                            this,                  SLOT(at_scene_doubleClicked(QGraphicsView *)));
    connect(dragInstrument,          SIGNAL(draggingStarted(QGraphicsView *, QList<QGraphicsItem *>)),  this,                  SLOT(at_draggingStarted(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(dragInstrument,          SIGNAL(draggingFinished(QGraphicsView *, QList<QGraphicsItem *>)), this,                  SLOT(at_draggingFinished(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(d->handScrollInstrument, SIGNAL(scrollingStarted(QGraphicsView *)),                         d->boundingInstrument, SLOT(dontEnforcePosition(QGraphicsView *)));
    connect(d->handScrollInstrument, SIGNAL(scrollingFinished(QGraphicsView *)),                        d->boundingInstrument, SLOT(enforcePosition(QGraphicsView *)));

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

        widget->setAnimationsEnabled(true);
        widget->setWindowTitle(SceneController::tr("item # %1").arg(i + 1));
        widget->setPos(QPointF(-500, -500));
        widget->setZValue(normalZ);

        QGraphicsGridLayout *layout = new QGraphicsGridLayout(widget);
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);

        LayoutItem *item = new LayoutItem(widget);
        layout->addItem(item, 0, 0, 1, 1, Qt::AlignCenter);
        item->setAcceptedMouseButtons(0);

        connect(widget, SIGNAL(resizingStarted()),  this, SLOT(at_widget_resizingStarted()));
        connect(widget, SIGNAL(resizingFinished()), this, SLOT(at_widget_resizingFinished()));

        d->animatedWidgets.push_back(widget);
    }

    /* Initialize view. */
    d->view = view;
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
    d->boundingInstrument->setSizeBounds(NULL, QSizeF(100, 100), InstrumentUtility::OutBound, d->centralWidget->size(), InstrumentUtility::OutBound);
}

void SceneController::at_widget_resizingStarted() {
    AnimatedWidget *widget = static_cast<AnimatedWidget *>(sender());
    widget->setAnimationsEnabled(false);
}

void SceneController::at_widget_resizingFinished() {
    Q_D(SceneController);

    AnimatedWidget *widget = static_cast<AnimatedWidget *>(sender());
    widget->setAnimationsEnabled(true);

    QRect newRect = d->centralLayout->mapToGrid(widget->geometry());
    QSet<QGraphicsLayoutItem *> items = d->centralLayout->itemsAt(newRect);
    items.remove(widget);
    if (items.empty())
        d->centralLayout->moveItem(widget, newRect);
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

        if(InstrumentUtility::contains(focusedWidgetRect.size(), viewportRect.size()) && !qFuzzyCompare(viewportRect, focusedWidgetRect)) {
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

void SceneController::at_zoomAnimation_finished() {
    d_func()->zoomAnimationFinished();
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
