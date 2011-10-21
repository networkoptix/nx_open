#include "scenecontroller.h"
#include <cassert>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsGridLayout>
#include <QGLWidget>
#include <QFile>
#include <QIcon>
#include <QAction>
#include <instruments/instrumentmanager.h>
#include <instruments/handscrollinstrument.h>
#include <instruments/wheelzoominstrument.h>
#include <instruments/rubberbandinstrument.h>
#include <instruments/draginstrument.h>
#include <instruments/contextmenuinstrument.h>
#include <instruments/clickinstrument.h>
#include <instruments/boundinginstrument.h>
#include <widgets/centralwidget.h>
#include <widgets/animatedwidget.h>
#include <widgets/layoutitem.h>
#include <widgets/celllayout.h>

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

} // anonymous namespace

SceneController::SceneController(QObject *parent):
    QObject(parent),
    m_scene(new QGraphicsScene(this)),
    m_manager(new InstrumentManager(this)),
    m_centralWidget(NULL),
    m_centralLayout(NULL),
    m_focusedWidget(NULL),
    m_focusedExpanded(false)
{
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex); // TODO: check.

    /* Install instruments. */
    m_manager->registerScene(m_scene);
    ClickInstrument *clickInstrument = new ClickInstrument(this);
    DragInstrument *dragInstrument = new DragInstrument(this);
    HandScrollInstrument *handScrollInstrument = new HandScrollInstrument(this);
    m_boundingInstrument = new BoundingInstrument(this);

    m_manager->installInstrument(new WheelZoomInstrument(this));
    m_manager->installInstrument(dragInstrument);
    m_manager->installInstrument(new RubberBandInstrument(this));
    m_manager->installInstrument(handScrollInstrument);
    m_manager->installInstrument(new ContextMenuInstrument(this));
    m_manager->installInstrument(clickInstrument);
    m_manager->installInstrument(m_boundingInstrument);

    connect(clickInstrument, SIGNAL(clicked(QGraphicsView *, QGraphicsItem *)), this, SLOT(at_clicked(QGraphicsView *, QGraphicsItem *)));
    connect(clickInstrument, SIGNAL(doubleClicked(QGraphicsView *, QGraphicsItem *)), this, SLOT(at_doubleClicked(QGraphicsView *, QGraphicsItem *)));

    connect(dragInstrument, SIGNAL(draggingStarted(QGraphicsView *, QList<QGraphicsItem *>)), this, SLOT(at_draggingStarted(QGraphicsView *, QList<QGraphicsItem *>)));
    connect(dragInstrument, SIGNAL(draggingFinished(QGraphicsView *, QList<QGraphicsItem *>)), this, SLOT(at_draggingFinished(QGraphicsView *, QList<QGraphicsItem *>)));

    connect(handScrollInstrument, SIGNAL(scrollingStarted(QGraphicsView *)), m_boundingInstrument, SLOT(dontEnforcePosition(QGraphicsView *)));
    connect(handScrollInstrument, SIGNAL(scrollingFinished(QGraphicsView *)), m_boundingInstrument, SLOT(enforcePosition(QGraphicsView *)));

    /* Create central widget. */
    m_centralWidget = new CentralWidget();
    m_centralWidget->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding, QSizePolicy::GroupBox));
    connect(m_centralWidget, SIGNAL(geometryChanged()), this, SLOT(at_centralWidget_geometryChanged()), Qt::QueuedConnection);
    m_scene->addItem(m_centralWidget);

    m_centralLayout = new CellLayout();
    m_centralLayout->setContentsMargins(spacing, spacing, spacing, spacing);
    m_centralLayout->setSpacing(spacing);
    m_centralLayout->setCellSize(200, 150);
    m_centralWidget->setLayout(m_centralLayout);

    /* Create items. */
    for (int i = 0; i < 16; ++i) {
        AnimatedWidget *widget = new AnimatedWidget(m_centralWidget);

        widget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true); /* Optimization. */
        widget->setFlag(QGraphicsItem::ItemIsSelectable, true);
        widget->setFlag(QGraphicsItem::ItemIsFocusable, true);

        widget->setAnimationsEnabled(true);
        widget->setWindowTitle(tr("item # %1").arg(i + 1));
        widget->setPos(QPointF(-500, -500));
        
        QGraphicsGridLayout *layout = new QGraphicsGridLayout(widget);
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);
        
        LayoutItem *item = new LayoutItem(widget);
        layout->addItem(item, 0, 0, 1, 1, Qt::AlignCenter);
        item->setAcceptedMouseButtons(0);

        connect(widget, SIGNAL(resizingStarted()),  this, SLOT(at_widget_resizingStarted()));
        connect(widget, SIGNAL(resizingFinished()), this, SLOT(at_widget_resizingFinished()));

        /*
        connect(widget, SIGNAL(clicked()), this, SLOT(itemClicked()));
        connect(widget, SIGNAL(doubleClicked()), this, SLOT(itemDoubleClicked()));
        connect(widget, SIGNAL(destroyed()), this, SLOT(itemDestroyed()));

        connect(widget, SIGNAL(clicked()), this, SLOT(_clicked()));
        connect(widget, SIGNAL(doubleClicked()), this, SLOT(_doubleClicked()));
        connect(widget, SIGNAL(movingStarted()), this, SLOT(at_draggingStarted()));
        connect(widget, SIGNAL(movingFinished()), this, SLOT(at_draggingFinished()));
        */

        m_animatedWidgets.push_back(widget);
    }


}

void SceneController::addView(QGraphicsView *view) {
    view->setScene(m_scene);
    view->setDragMode(QGraphicsView::NoDrag);

    m_boundingInstrument->setSizeEnforced(view, true);
    m_boundingInstrument->setPositionEnforced(view, true);
    m_boundingInstrument->setPositionBounds(view, QRectF(0, 0, 1000, 1000), 0.0);
    m_boundingInstrument->setSizeBounds(view, QSizeF(1000, 1000));
    m_boundingInstrument->setScalingSpeed(view, 8.0);
    m_boundingInstrument->setMovementSpeed(view, 4.0);

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
    view->setOptimizationFlag(QGraphicsView::DontSavePainterState); 
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

    m_manager->registerView(view);
}

QGraphicsScene *SceneController::scene() const {
    return m_scene;
}

SceneController::Mode SceneController::mode() const {
    return m_mode;
}

void SceneController::setMode(Mode mode) {
    if(mode == m_mode)
        return;

    m_mode = mode;
    foreach (AnimatedWidget *widget, m_animatedWidgets)
        widget->setInteractive(mode == EDITING);
    m_centralWidget->setGridVisible(mode == EDITING);
}

void SceneController::at_relayoutAction_triggered() {
    QAction *action = qobject_cast<QAction *>(sender());
    if(action == NULL)
        return; /* Should never happen, actually. */

    relayoutItems(action->property("rowCount").toInt(), action->property("columnCount").toInt(), action->property("preset").toByteArray());
}

void SceneController::at_centralWidget_geometryChanged() {
    // ### optimize/re-work

#if 0
    const QRectF viewportSceneRect = mapRectToScene(viewport()->rect());
    //const QSizeF s = (viewportSceneRect.size() - m_widget->mapRectToScene(m_widget->rect()).size()) / 2;
    const QSizeF s = (viewportSceneRect.size() - m_widget->size()) / 2;
    const QPointF newPos = viewportSceneRect.topLeft() + QPointF(s.width(), s.height());

    QPropertyAnimation *posAnimation = qobject_cast<QPropertyAnimation *>(m_widget->property("animation").value<QObject *>());
    if (!posAnimation)
    {
        posAnimation = new QPropertyAnimation(m_widget, "pos", m_widget);
        posAnimation->setDuration(150);
        posAnimation->setEasingCurve(QEasingCurve::Linear);
        m_widget->setProperty("animation", QVariant::fromValue(qobject_cast<QObject *>(posAnimation)));
    }
    if (posAnimation->endValue().toPointF() != newPos)
    {
        posAnimation->stop();
        posAnimation->setEndValue(newPos);
        posAnimation->start();
    }
#endif

}

void SceneController::at_widget_resizingStarted() {
    AnimatedWidget *widget = static_cast<AnimatedWidget *>(sender());
    widget->setAnimationsEnabled(false);
}

void SceneController::at_widget_resizingFinished() {
    AnimatedWidget *widget = static_cast<AnimatedWidget *>(sender());
    widget->setAnimationsEnabled(true);

    QRect newRect = m_centralLayout->mapToGrid(widget->geometry());
    QSet<QGraphicsLayoutItem *> items = m_centralLayout->itemsAt(newRect);
    items.remove(widget);
    if (items.empty())
        m_centralLayout->moveItem(widget, newRect);
}

void SceneController::at_draggingStarted(QGraphicsView *, QList<QGraphicsItem *> items) {
    foreach (QGraphicsItem *item, items)
        if (AnimatedWidget *widget = dynamic_cast<AnimatedWidget *>(item))
            widget->setAnimationsEnabled(false);
}

void SceneController::at_draggingFinished(QGraphicsView *view, QList<QGraphicsItem *> items) {
    AnimatedWidget *draggedWidget = NULL;
    foreach (QGraphicsItem *item, items) {
        draggedWidget = dynamic_cast<AnimatedWidget *>(item);
        if (draggedWidget != NULL)
            draggedWidget->setAnimationsEnabled(true);
    }

    if(draggedWidget == NULL)
        return;

    CellLayout *layout = m_centralLayout;
    QPoint delta = layout->mapToGrid(draggedWidget->geometry()).topLeft() - layout->rect(draggedWidget).topLeft();

    QList<QGraphicsLayoutItem *> layoutItems;
    QList<QRect> rects;
    foreach (QGraphicsItem *item, items)
        if (QGraphicsLayoutItem *layoutItem = dynamic_cast<QGraphicsLayoutItem *>(item))
            layoutItems.push_back(layoutItem);

    QGraphicsLayoutItem *replacedItem = layout->itemAt(layout->mapToGrid(m_centralWidget->mapFromScene(view->mapToScene(view->mapFromGlobal(QCursor::pos())))));
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

void SceneController::at_clicked(QGraphicsView *view, QGraphicsItem *item) {
    AnimatedWidget *widget = dynamic_cast<AnimatedWidget *>(item);
    if(widget == NULL)
        return;

    if (widget == m_focusedWidget) {
        m_focusedExpanded = !m_focusedExpanded;
        repositionFocusedWidget(view);
        return;
    }

    if (m_focusedWidget != NULL) {
        m_focusedWidget = NULL;
        invalidateLayout();
    }

    m_focusedWidget = widget;
    m_focusedExpanded = true;
    repositionFocusedWidget(view);


/*    if (scene()->selectedItems().isEmpty()) {
        invalidateLayout();

        m_focusedWidget = NULL;
    }*/
}

void SceneController::repositionFocusedWidget(QGraphicsView *view) {
    if(m_focusedExpanded) {
        QRectF viewportRect = mapRectToScene(view, view->viewport()->rect());
        QRectF widgetRect = m_centralLayout->mapFromGrid(m_centralLayout->rect(m_focusedWidget));

        QPointF viewportCenter = viewportRect.center();
        QPointF widgetCenter = widgetRect.center();

        // TODO: move up.
        static const qreal focusExpansion = 1.5;
        static const qreal maxExpandedSize = 0.8;

        QSizeF newWidgetSize = widgetRect.size() * focusExpansion;
        QSizeF maxWidgetSize = viewportRect.size() * maxExpandedSize;

        /* Allow expansion no further than the maximal size, but no less than current size. */
        newWidgetSize =  bounded(newWidgetSize, maxWidgetSize,     Qt::KeepAspectRatio);
        newWidgetSize = expanded(newWidgetSize, widgetRect.size(), Qt::KeepAspectRatio);
        
        /* Calculate expansion values. Expand towards the screen center. */
        qreal xp1 = 0.0, xp2 = 0.0, yp1 = 0.0, yp2 = 0.0;
        calculateExpansionValues(widgetRect.left(), widgetRect.right(),  viewportCenter.x(), newWidgetSize.width(),  &xp1, &xp2);
        calculateExpansionValues(widgetRect.top(),  widgetRect.bottom(), viewportCenter.y(), newWidgetSize.height(), &yp1, &yp2);
        QRectF newWidgetRect = widgetRect.adjusted(xp1, yp1, xp2, yp2);

        m_focusedWidget->setGeometry(newWidgetRect);
        m_focusedWidget->setZValue(m_focusedWidget->zValue() + 1);
    } else {
        invalidateLayout();
    }
}

void SceneController::at_doubleClicked(QGraphicsView *view, QGraphicsItem *item)
{
    if (QGraphicsWidget *widget = dynamic_cast<QGraphicsWidget *>(item))
    {
        const QRectF viewportSceneRect = view->viewportTransform().inverted().mapRect(view->viewport()->rect()).adjusted(2, 2, -2, -2);
        const QRectF newGeom = widget->mapRectToParent(widget->mapRectFromScene(viewportSceneRect));
        widget->setGeometry(newGeom);
        widget->setZValue(widget->zValue() + 1);
    }
}

void SceneController::at_widget_destroyed()
{
    m_animatedWidgets.removeOne(static_cast<AnimatedWidget *>(sender()));
    //QTimer::singleShot(250, this, SLOT(relayoutItemsActionTriggered()));
}


void SceneController::relayoutItems(int rowCount, int columnCount, const QByteArray &preset) {
    QByteArray localPreset = preset.size() == rowCount * columnCount ? preset : QByteArray(rowCount * columnCount, '1');

    while(m_centralLayout->count() != 0)
        m_centralLayout->removeAt(0);

    int i = 0;
    for (int row = 0; row < rowCount; ++row) {
        for (int column = 0; column < columnCount; ++column) {
            if (QGraphicsWidget *widget = m_animatedWidgets.value(i, 0)) {
                const int span = localPreset.at(row * columnCount + column) - '0';
                if (span > 0) {
                    m_centralLayout->addItem(widget, row, column, span, span, Qt::AlignCenter);
                    ++i;
                }
            }
        }
    }
    
    while (i < m_animatedWidgets.size()) {
        QGraphicsWidget *widget = m_animatedWidgets.at(i++);
        widget->setGeometry(QRectF(QPointF(-500, -500), widget->preferredSize()));
    }
}

void SceneController::invalidateLayout() {
    m_centralLayout->invalidate();
    m_centralLayout->activate();
}

