#include "graphicsview.h"

#include <cmath> /* For std::pow. */

#include <QtCore/QPropertyAnimation>

#include <QtGui/QGraphicsScene>

#ifndef QT_NO_OPENGL
#  include <QtOpenGL>
#endif

#include "animatedwidget.h"
#include "celllayout.h"
#include "centralwidget.h"
#include "layoutitem.h"

#include "graphicswidget.h"

static const qreal spacing = 25;


GraphicsView::GraphicsView(QWidget *parent)
    : QGraphicsView(parent),
      m_widget(0),
      m_selectedWidget(0)
#if 0
      mDragState(INITIAL)
#endif
{
    {
        QGraphicsScene *scene = new QGraphicsScene(this);
        scene->setItemIndexMethod(QGraphicsScene::NoIndex);
        //scene->setStyle(..); // scene-specific style
        setScene(scene);
    }

#ifndef QT_NO_OPENGL
    if (QGLFormat::hasOpenGL())
    {
        QGLFormat glFormat(QGL::SampleBuffers);
        glFormat.setSwapInterval(1); // vsync

        setViewport(new QGLWidget(glFormat)); // antialiasing
    }
    else
#endif
    {
        setViewport(new QWidget);
    }

    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing); // antialiasing
    setRenderHints(QPainter::SmoothPixmapTransform);

    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

    setOptimizationFlag(QGraphicsView::DontSavePainterState, false); // ### could be optimized
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing);

    //setCacheMode(QGraphicsView::CacheBackground); // slows down scene drawing a lot!!!

    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    //setTransformationAnchor(QGraphicsView::NoAnchor);
    //setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    //setResizeAnchor(QGraphicsView::NoAnchor);
    //setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

//    setAlignment(Qt::AlignVCenter);

/*    QPalette palette;
    palette.setColor(backgroundRole(), QColor(0, 5, 5, 125));
    setPalette(palette);*/

    setAcceptDrops(true);

    setMinimumSize(600, 400);

    setFrameShape(QFrame::NoFrame);


    createActions();

    m_widget = new CentralWidget;
//    m_widget->setWindowFlags(Qt::Tool);
    m_widget->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding, QSizePolicy::GroupBox));

    connect(m_widget, SIGNAL(geometryChanged()), this, SLOT(widgetGeometryChanged()), Qt::QueuedConnection);

    for (int i = 0; i < 16; ++i)
    {
        AnimatedWidget *widget = new AnimatedWidget(m_widget);



//        widget->setFlag(QGraphicsItem::ItemHasNoContents, true); // optimization
        widget->setFlag(QGraphicsItem::ItemIgnoresParentOpacity, true); // optimization
        widget->setFlag(QGraphicsItem::ItemIsSelectable, true);
        widget->setFlag(QGraphicsItem::ItemIsFocusable, true);
        //widget->setExtraFlag(GraphicsWidget::ItemIsResizable, true);
        //widget->setExtraFlag(GraphicsWidget::ItemIsDraggable, true);
        widget->setAnimationsEnabled(true);
//        widget->setFlag(QGraphicsItem::ItemIsFocusScope, true);
//        widget->setFlag(QGraphicsItem::ItemIsPanel, false);
//        widget->setFocusPolicy(Qt::TabFocus);
        widget->setWindowTitle(tr("item # %1").arg(i + 1));
        widget->setPos(QPointF(-500, -500));
        scene()->addItem(widget);

        QGraphicsGridLayout *layout = new QGraphicsGridLayout(widget);
        layout->setSpacing(0);
        layout->setContentsMargins(0, 0, 0, 0);
        LayoutItem *item = new LayoutItem(widget);
//        item->setFlags(item->flags() | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
        layout->addItem(item, 0, 0, 1, 1, Qt::AlignCenter);

        connect(widget, SIGNAL(clicked()), this, SLOT(itemClicked()));
        connect(widget, SIGNAL(doubleClicked()), this, SLOT(itemDoubleClicked()));
        connect(widget, SIGNAL(destroyed()), this, SLOT(itemDestroyed()));

        connect(widget, SIGNAL(clicked()), this, SLOT(_clicked()));
        connect(widget, SIGNAL(doubleClicked()), this, SLOT(_doubleClicked()));
        connect(widget, SIGNAL(resizingStarted()), this, SLOT(_resizingStarted()));
        connect(widget, SIGNAL(resizingFinished()), this, SLOT(_resizingFinished()));
        connect(widget, SIGNAL(movingStarted()), this, SLOT(_draggingStarted()));
        connect(widget, SIGNAL(movingFinished()), this, SLOT(_draggingFinished()));

        m_animatedWidgets.append(widget);
    }

    scene()->addItem(m_widget);
    scene()->setSceneRect(0, 0, 1280, 1024);
    m_widget->setGeometry(0, 0, 900, 900);

    relayoutItems(4, 4);

#if 0
    NotifyingWidget *ww[2] = {new NotifyingWidget(), new NotifyingWidget()};

    for(int i = 0; i < 2; i++) 
    {
        NotifyingWidget *w = ww[i];

        scene()->addItem(w);
        w->setAutoFillBackground(true);
        w->setGeometry(50 + i * 150, 50, 100, 100);
        w->setWindowFlags(Qt::Window);
        w->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);

        connect(w, SIGNAL(clicked()), this, SLOT(_clicked()));
        connect(w, SIGNAL(doubleClicked()), this, SLOT(_doubleClicked()));
        connect(w, SIGNAL(resizingStarted()), this, SLOT(_resizingStarted()));
        connect(w, SIGNAL(resizingFinished()), this, SLOT(_resizingFinished()));
        connect(w, SIGNAL(draggingStarted()), this, SLOT(_draggingStarted()));
        connect(w, SIGNAL(draggingFinished()), this, SLOT(_draggingFinished()));
    }
#endif
}

GraphicsView::~GraphicsView()
{
    delete m_widget;
}

void GraphicsView::_clicked() {
    qDebug() << "clicked";
}

void GraphicsView::_doubleClicked() {
    qDebug() << "doubleClicked";
}

void GraphicsView::_resizingStarted() {
    qDebug() << "resizingStarted";

    AnimatedWidget *item = static_cast<AnimatedWidget *>(sender());
    item->setAnimationsEnabled(false);
}

void GraphicsView::_resizingFinished() {
    qDebug() << "resizingFinished";

    AnimatedWidget *item = static_cast<AnimatedWidget *>(sender());
    CellLayout *layout = static_cast<CellLayout *>(m_widget->layout());

    item->setAnimationsEnabled(true);

    QRect newRect = layout->mapToGrid(item->geometry());

    QSet<QGraphicsLayoutItem *> items = layout->itemsAt(newRect);
    items.remove(item);

    if(items.empty())
        layout->moveItem(item, newRect);
}

void GraphicsView::_draggingStarted() {
    qDebug() << "draggingStarted";

    AnimatedWidget *draggedItem = static_cast<AnimatedWidget *>(sender());
    
    draggedItem->setAnimationsEnabled(false);
    foreach (QGraphicsItem *item, scene()->selectedItems())
        if(AnimatedWidget *widget = dynamic_cast<AnimatedWidget *>(item))
            widget->setAnimationsEnabled(false);
}

void GraphicsView::_draggingFinished() {
    qDebug() << "draggingFinished";

    // TODO: duplicate code.

    AnimatedWidget *draggedItem = static_cast<AnimatedWidget *>(sender());
    CellLayout *layout = static_cast<CellLayout *>(m_widget->layout());

    draggedItem->setAnimationsEnabled(true);
    foreach (QGraphicsItem *item, scene()->selectedItems())
        if(AnimatedWidget *widget = dynamic_cast<AnimatedWidget *>(item))
            widget->setAnimationsEnabled(true);

    QPoint delta = layout->mapToGrid(draggedItem->geometry()).topLeft() - layout->rect(draggedItem).topLeft();

    QList<QGraphicsLayoutItem *> items;
    foreach (QGraphicsItem *item, scene()->selectedItems())
        if(QGraphicsLayoutItem *layoutItem = dynamic_cast<QGraphicsLayoutItem *>(item))
            items.push_back(layoutItem);
    if(!items.contains(draggedItem))
        items.push_back(draggedItem);

    QList<QRect> rects;
    foreach (QGraphicsLayoutItem *layoutItem, items)
        rects.push_back(layout->rect(layoutItem).adjusted(delta.x(), delta.y(), delta.x(), delta.y()));

    QList<QGraphicsLayoutItem *> replacedItems = layout->itemsAt(rects).subtract(items.toSet()).toList();
    QList<QRect> replacedRects;
    foreach (QGraphicsLayoutItem *layoutItem, replacedItems)
        replacedRects.push_back(layout->rect(layoutItem).adjusted(-delta.x(), -delta.y(), -delta.x(), -delta.y()));

    items.append(replacedItems);
    rects.append(replacedRects);

    layout->moveItems(items, rects);
    layout->invalidate();
    layout->activate();
}


bool GraphicsView::isEditMode() const
{
    return !m_animatedWidgets.isEmpty() && static_cast<AnimatedWidget *>(m_animatedWidgets.first())->isInteractive();
}

void GraphicsView::setEditMode(bool interactive)
{
    foreach (QGraphicsWidget *widget, m_animatedWidgets)
        static_cast<AnimatedWidget *>(widget)->setInteractive(interactive);
    m_widget->setGridVisible(interactive);
}

void GraphicsView::createActions()
{
    // ### re-work layout presets format and handler code
    {
        QByteArray preset =
                "4000"
                "0000"
                "0000"
                "0000";

        QAction *action = new QAction(QIcon(QLatin1String(":/images/1.bmp")), tr(""), this);
        action->setShortcut(QKeySequence(Qt::Key_F1));
        action->setShortcutContext(Qt::WindowShortcut);
        action->setAutoRepeat(false);
        action->setProperty("rowCount", 4);
        action->setProperty("columnCount", 4);
        action->setProperty("preset", preset);
        connect(action, SIGNAL(triggered()), this, SLOT(relayoutItemsActionTriggered()));
        addAction(action);
    }
    {
        QByteArray preset =
                "2020"
                "0000"
                "2020"
                "0000";

        QAction *action = new QAction(QIcon(QLatin1String(":/images/1-1-1-1.bmp")), tr(""), this);
        action->setShortcut(QKeySequence(Qt::Key_F2));
        action->setShortcutContext(Qt::WindowShortcut);
        action->setAutoRepeat(false);
        action->setProperty("rowCount", 4);
        action->setProperty("columnCount", 4);
        action->setProperty("preset", preset);
        connect(action, SIGNAL(triggered()), this, SLOT(relayoutItemsActionTriggered()));
        addAction(action);
    }
    {
        QByteArray preset =
                "2020"
                "0000"
                "2011"
                "0011";

        QAction *action = new QAction(QIcon(QLatin1String(":/images/1-1-1-4.bmp")), tr(""), this);
        action->setShortcut(QKeySequence(Qt::Key_F3));
        action->setShortcutContext(Qt::WindowShortcut);
        action->setAutoRepeat(false);
        action->setProperty("rowCount", 4);
        action->setProperty("columnCount", 4);
        action->setProperty("preset", preset);
        connect(action, SIGNAL(triggered()), this, SLOT(relayoutItemsActionTriggered()));
        addAction(action);
    }
    {
        QByteArray preset =
                "2011"
                "0011"
                "2011"
                "0011";

        QAction *action = new QAction(QIcon(QLatin1String(":/images/1-4-1-4.bmp")), tr(""), this);
        action->setShortcut(QKeySequence(Qt::Key_F4));
        action->setShortcutContext(Qt::WindowShortcut);
        action->setAutoRepeat(false);
        action->setProperty("rowCount", 4);
        action->setProperty("columnCount", 4);
        action->setProperty("preset", preset);
        connect(action, SIGNAL(triggered()), this, SLOT(relayoutItemsActionTriggered()));
        addAction(action);
    }
    {
        QByteArray preset =
                "2011"
                "0011"
                "1111"
                "1111";

        QAction *action = new QAction(QIcon(QLatin1String(":/images/1-4-4-4.bmp")), tr(""), this);
        action->setShortcut(QKeySequence(Qt::Key_F5));
        action->setShortcutContext(Qt::WindowShortcut);
        action->setAutoRepeat(false);
        action->setProperty("rowCount", 4);
        action->setProperty("columnCount", 4);
        action->setProperty("preset", preset);
        connect(action, SIGNAL(triggered()), this, SLOT(relayoutItemsActionTriggered()));
        addAction(action);
    }
    {
        QByteArray preset =
                "3001"
                "0001"
                "0001"
                "1111";

        QAction *action = new QAction(QIcon(QLatin1String(":/images/1x3+.bmp")), tr(""), this);
        action->setShortcut(QKeySequence(Qt::Key_F6));
        action->setShortcutContext(Qt::WindowShortcut);
        action->setAutoRepeat(false);
        action->setProperty("rowCount", 4);
        action->setProperty("columnCount", 4);
        action->setProperty("preset", preset);
        connect(action, SIGNAL(triggered()), this, SLOT(relayoutItemsActionTriggered()));
        addAction(action);
    }
    {
        QByteArray preset =
                "1111"
                "1201"
                "1001"
                "1111";

        QAction *action = new QAction(QIcon(), tr("1"), this);
        action->setShortcut(QKeySequence(Qt::Key_F7));
        action->setShortcutContext(Qt::WindowShortcut);
        action->setAutoRepeat(false);
        action->setProperty("rowCount", 4);
        action->setProperty("columnCount", 4);
        action->setProperty("preset", preset);
        connect(action, SIGNAL(triggered()), this, SLOT(relayoutItemsActionTriggered()));
        addAction(action);
    }
    {
        QByteArray preset =
                "2020"
                "0000"
                "1201"
                "1001";

        QAction *action = new QAction(QIcon(), tr("2"), this);
        action->setShortcut(QKeySequence(Qt::Key_F8));
        action->setShortcutContext(Qt::WindowShortcut);
        action->setAutoRepeat(false);
        action->setProperty("rowCount", 4);
        action->setProperty("columnCount", 4);
        action->setProperty("preset", preset);
        connect(action, SIGNAL(triggered()), this, SLOT(relayoutItemsActionTriggered()));
        addAction(action);
    }
    {
        QByteArray preset =
                "1111"
                "1111"
                "1111"
                "1111";

        QAction *action = new QAction(QIcon(QLatin1String(":/images/4-4-4-4.png")), tr(""), this);
        action->setShortcut(QKeySequence(Qt::Key_F9));
        action->setShortcutContext(Qt::WindowShortcut);
        action->setAutoRepeat(false);
        action->setProperty("rowCount", 4);
        action->setProperty("columnCount", 4);
        action->setProperty("preset", preset);
        connect(action, SIGNAL(triggered()), this, SLOT(relayoutItemsActionTriggered()));
        addAction(action);
    }
}

void GraphicsView::relayoutItems(int rowCount, int columnCount, const QByteArray &preset_)
{
    QByteArray preset = preset_.size() == rowCount * columnCount ? preset_ : QByteArray(rowCount * columnCount, '1');

    m_widget->setLayout(0);

    CellLayout *layout = new CellLayout;
    layout->setContentsMargins(spacing, spacing, spacing, spacing);
    layout->setSpacing(spacing);
    layout->setCellSize(200, 150);

    m_widget->setLayout(layout);
    m_widget->setProperty("preset", preset);

    int i = 0;
    for (int row = 0; row < rowCount; ++row)
    {
        for (int column = 0; column < columnCount; ++column)
        {
            if (QGraphicsWidget *widget = m_animatedWidgets.value(i, 0))
            {
                const int span = preset.at(row * columnCount + column) - '0';
                if (span > 0)
                {
                    /*if (layout->count())
                        QGraphicsWidget::setTabOrder(static_cast<QGraphicsWidget *>(layout->itemAt(layout->count() - 1)), widget);*/

                    layout->addItem(widget, row, column, span, span, Qt::AlignCenter);
                    ++i;
                }
            }
        }
    }
    /*if (layout->count() > 1)
        QGraphicsWidget::setTabOrder(static_cast<QGraphicsWidget *>(layout->itemAt(layout->count() - 1)), static_cast<QGraphicsWidget *>(layout->itemAt(0)));*/

    while (i < m_animatedWidgets.size())
    {
        QGraphicsWidget *widget = m_animatedWidgets.at(i++);
        widget->setGeometry(QRectF(QPointF(-500, -500), widget->preferredSize()));
    }
}

void GraphicsView::invalidateLayout()
{
    if (QGraphicsLayout *layout = m_widget->layout())
    {
        /*foreach (QGraphicsWidget *widget, m_animatedWidgets)
            widget->setZValue(m_widget->zValue());*/
        layout->invalidate();
        layout->activate();
    }
}

void GraphicsView::relayoutItemsActionTriggered()
{
    if (QAction *action = qobject_cast<QAction *>(sender()))
        relayoutItems(action->property("rowCount").toInt(), action->property("columnCount").toInt(), action->property("preset").toByteArray());
    else if (CellLayout *layout = static_cast<CellLayout *>(m_widget->layout()))
        relayoutItems(layout->rowCount(), layout->columnCount(), m_widget->property("preset").toByteArray());
}

void GraphicsView::widgetGeometryChanged()
{
    // ### optimize/re-work
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
}

void GraphicsView::itemClicked()
{
    if (AnimatedWidget *widget = qobject_cast<AnimatedWidget *>(sender()))
    {
        if(widget == m_selectedWidget)
            return;

        if(m_selectedWidget != NULL) {
            m_selectedWidget = NULL;
            invalidateLayout();
        }

        CellLayout *layout = static_cast<CellLayout *>(m_widget->layout());

        QRectF widgetRect = layout->mapFromGrid(layout->rect(widget));

        const QRectF viewportSceneRect = mapRectToScene(viewport()->rect());
        const QSizeF s = (viewportSceneRect.size() - widget->mapRectToScene(widgetRect).size() * 2.5) / 2;
        const QPointF newPos = viewportSceneRect.topLeft() + QPointF(s.width(), s.height());
        const QRectF newGeom = QRectF(widget->mapToParent(widget->mapFromScene(newPos)), widgetRect.size() * 2.5);
        widget->setGeometry(newGeom);
        widget->setZValue(widget->zValue() + 1);

        m_selectedWidget = widget;
    }
}

void GraphicsView::itemDoubleClicked()
{
    if (QGraphicsWidget *widget = qobject_cast<QGraphicsWidget *>(sender()))
    {
        const QRectF viewportSceneRect = mapRectToScene(viewport()->rect()).adjusted(2, 2, -2, -2);
        const QRectF newGeom = widget->mapRectToParent(widget->mapRectFromScene(viewportSceneRect));
        widget->setGeometry(newGeom);
        widget->setZValue(widget->zValue() + 1);
    }
}

void GraphicsView::itemDestroyed()
{
    m_animatedWidgets.removeOne(static_cast<QGraphicsWidget *>(sender()));
    QTimer::singleShot(250, this, SLOT(relayoutItemsActionTriggered()));
}

void GraphicsView::keyPressEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Plus:
    case Qt::Key_Minus:
    case Qt::Key_Asterisk:
        if ((event->modifiers() & Qt::KeypadModifier) == Qt::KeypadModifier)
        {
            m_widget->setTransformOriginPoint(m_widget->rect().center());
            if (event->key() == Qt::Key_Asterisk)
            {
                m_widget->setRotation(0.0);
                m_widget->setScale(1.0);
            }
            else
            {
                if ((event->modifiers() & Qt::AltModifier) == Qt::AltModifier)
                    m_widget->setRotation(m_widget->rotation() + 15 * (event->key() == Qt::Key_Minus ? -1 : 1));
                else
                    m_widget->setScale(m_widget->scale() + 0.25 * (event->key() == Qt::Key_Minus ? -1 : 1));
            }
        }
        break;

    case Qt::Key_Control:
        setDragMode(QGraphicsView::RubberBandDrag);
        scene()->clearSelection();
        setEditMode(true);
        break;

    case Qt::Key_Space:
        invalidateLayout();
        break;

    case Qt::Key_Escape:
        if (QWidget *window = topLevelWidget()->window())
        {
            if (!window->isFullScreen())
                window->showFullScreen();
            else
                window->showMaximized();
        }
        break;

    default:
        break;
    }

    QGraphicsView::keyPressEvent(event);
}

void GraphicsView::keyReleaseEvent(QKeyEvent *event)
{
    switch (event->key())
    {
    case Qt::Key_Control:
        setDragMode(QGraphicsView::NoDrag);
        scene()->clearSelection();
        setEditMode(false);
        break;

    default:
        break;
    }

    QGraphicsView::keyReleaseEvent(event);
}


void GraphicsView::mousePressEvent(QMouseEvent *event)
{
    QGraphicsView::mousePressEvent(event);

#if 1
    if(mState != INITIAL)
        return;

    if(event->button() != Qt::RightButton)
        return;

    mState = PREPAIRING;
    mMousePressPos = event->pos();
    mLastMousePos = event->pos();

    event->accept();
#endif
}

void GraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    QGraphicsView::mouseMoveEvent(event);

#if 1
    if(mState == INITIAL)
        return;

    /* Stop scrolling if the user has let go of the trigger button (even if we didn't get the release events). */
    if(!(event->buttons() & Qt::RightButton)) {
        if(mState == SCROLLING)
            viewport()->setCursor(mOriginalCursor);
        mState = INITIAL;
        return;
    }

    /* Check for drag distance. */
    if(mState == PREPAIRING) {
        if((mMousePressPos - event->pos()).manhattanLength() < QApplication::startDragDistance()) {
            return;
        } else {
            mOriginalCursor = viewport()->cursor();
            viewport()->setCursor(Qt::ClosedHandCursor);
            mState = SCROLLING;
        }
    }

    QScrollBar *hBar = horizontalScrollBar();
    QScrollBar *vBar = verticalScrollBar();
    QPoint delta = event->pos() - mLastMousePos;
    hBar->setValue(hBar->value() + (isRightToLeft() ? delta.x() : -delta.x()));
    vBar->setValue(vBar->value() - delta.y());

    mLastMousePos = event->pos();

    event->accept();
#endif
}

void GraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    QGraphicsView::mouseReleaseEvent(event);

    if (scene()->selectedItems().isEmpty()) {
        invalidateLayout();

        m_selectedWidget = NULL;
    }

#if 1
    if(mState == INITIAL)
        return;

    if(event->button() != Qt::RightButton)
        return;

    if(mState == SCROLLING)
        viewport()->setCursor(mOriginalCursor);
    mState = INITIAL;

    event->accept();
#endif
}

void GraphicsView::wheelEvent(QWheelEvent *event) {
    /* delta() returns the distance that the wheel is rotated
     * in eighths (1/8s) of a degree. */
    qreal degrees = event->delta() / 8.0;

    /* Use 2x scale for 180 degree turn. */
    qreal scaleFactor = std::pow(2.0, degrees / 180.0);

    /* Scale restoring the transformation anchor. */
    QGraphicsView::ViewportAnchor oldAnchor = transformationAnchor();
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    scale(scaleFactor, scaleFactor);
    setTransformationAnchor(oldAnchor);

    event->accept();
}