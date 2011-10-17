#include "rubberbandinstrument.h"
#include <cassert>
#include <limits>
#include <QGraphicsView>
#include <QMouseEvent>
#include <QApplication>
#include <QGraphicsItem>
#include <QStyleHintReturnMask>
#include <QStyleOptionRubberBand>
#include <QRubberBand>
#include <QWidget>
#include <QCursor>

/**
 * Item that implements a rubber band. The downside of implementing rubber band
 * as an item is that <tt>QGraphicsView</tt>'s foreground will be drawn atop of it.
 * 
 * Unfortunately, it is impossible to hook into QGraphicsView's rendering
 * pipeline except by subclassing it, so this is probably the best non-intrusive solution.
 */
class RubberBandItem: public QGraphicsItem {
public:
    RubberBandItem(QGraphicsScene *scene = NULL): 
        QGraphicsItem(NULL, scene),
        mViewport(NULL),
        mRectsDirty(true)
    {
        setAcceptedMouseButtons(0);
        setEnabled(false);
        setZValue(std::numeric_limits<qreal>::max());
    }

    virtual QRectF boundingRect() const override {
        if (mViewport == NULL)
            return QRectF();

        ensureRects();

        return mBoundingRect;
    }

    /* We can also implement shape(), but most probably there is no point. */

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) override {
        if (widget != mViewport)
            return; /* Draw it on source viewport only. */

        ensureRects();

        if (mRubberBandRect.isEmpty())
            return;

        QStyleOptionRubberBand rubberBandOption;
        rubberBandOption.initFrom(widget);
        rubberBandOption.rect = mRubberBandRect;
        rubberBandOption.shape = QRubberBand::Rectangle;

        QRegion oldClipRegion;
        QStyleHintReturnMask mask;
        if (widget->style()->styleHint(QStyle::SH_RubberBand_Mask, &rubberBandOption, widget, &mask)) {
            /* Painter clipping for masked rubber bands. */
            oldClipRegion = painter->clipRegion();
            painter->setClipRegion(mask.region, Qt::IntersectClip);
        }

        QTransform oldTransform = painter->transform();
        painter->resetTransform();

        widget->style()->drawControl(QStyle::CE_RubberBand, &rubberBandOption, painter, widget);

        /* Restore painter state. */
        painter->setTransform(oldTransform);
        if (!oldClipRegion.isEmpty())
            painter->setClipRegion(oldClipRegion);
    }

    /**
     * Sets this item's viewport. This item will be drawn only on the given
     * viewport. This item won't access the given viewport in any way, so it is
     * safe to delete the viewport without notifying the item.
     * 
     * \param viewport                  Viewport to draw this item on.
     */
    void setViewport(QWidget *viewport) {
        prepareGeometryChange();
        mViewport = viewport;
    }

    /**
     * \returns                         This rubber band item's viewport.
     */
    QWidget *viewport() const {
        return mViewport;
    }

    /**
     * \param sceneToViewport           Scene-to-viewport transformation for the viewport 
     *                                  that this item will be drawn on.
     */
    void setViewportTransform(const QTransform &sceneToViewport) {
        if (sceneToViewport == mSceneToViewport)
            return;

        mSceneToViewport = sceneToViewport;
        updateRects();
    }

    void setOrigin(const QPointF &origin) {
        mOrigin = origin;

        updateRects();
    }

    void setCorner(const QPointF &corner) {
        mCorner = corner;

        updateRects();
    }

    /**
     * \returns                         Current rubber band rectangle in view coordinates.
     */
    const QRect &rubberBandRect() const {
        ensureRects();

        return mRubberBandRect;
    }

protected:
    void updateRects() {
        prepareGeometryChange();

        mRectsDirty = true;
    }

    void ensureRects() const {
        if (!mRectsDirty)
            return;

        /* Create scene-to-viewport transformation. */
        QTransform viewportToScene = mSceneToViewport.inverted();

        mRubberBandRect = QRectF(mSceneToViewport.map(mOrigin), mSceneToViewport.map(mCorner)).normalized().toRect();
        mBoundingRect = viewportToScene.mapRect(QRectF(mRubberBandRect));
        mRectsDirty = false;
    }

private:
    /** Viewport that this rubber band will be drawn at. */
    QWidget *mViewport;

    /** Whether stored rectangles need recalculating. */
    mutable bool mRectsDirty;

    /** Current rubber band rectangle, in viewport coordinates. */
    mutable QRect mRubberBandRect;

    /** Bounding rect of the current rubber band rectangle, in scene coordinates. */
    mutable QRectF mBoundingRect;

    /** Origin of the rubber band, in scene coordinates. */
    QPointF mOrigin;

    /** Second corner of the rubber band, in scene coordinates. */
    QPointF mCorner;

    /** Current viewport-to-scene transformation. */
    QTransform mSceneToViewport;
};


namespace {
    QEvent::Type viewportEventTypes[] = {
        QEvent::MouseButtonPress,
        QEvent::MouseMove,
        QEvent::MouseButtonRelease,
        QEvent::Paint
    };
}

RubberBandInstrument::RubberBandInstrument(QObject *parent):
    Instrument(makeSet(), makeSet(), makeSet(viewportEventTypes), false, parent)
{}

RubberBandInstrument::~RubberBandInstrument() {
    ensureUninstalled();
}

void RubberBandInstrument::installedNotify() {
    mState = INITIAL;
    
    mRubberBand.reset(new RubberBandItem(scene()));

    Instrument::installedNotify();
}

void RubberBandInstrument::aboutToBeUninstalledNotify() {
    if (scene() == NULL) {
        /* Rubber band item will be deleted by scene, just zero the pointer. */
        mRubberBand.take();
    } else {
        mRubberBand.reset();
    }
}

bool RubberBandInstrument::paintEvent(QWidget *viewport, QPaintEvent *) {
    if (mState != RUBBER_BANDING)
        return false;

    QGraphicsView *view = this->view(viewport);
    mRubberBand->setViewportTransform(view->viewportTransform());

    /* Scene mouse position may have changed as a result of transform change. */
    mRubberBand->setCorner(view->mapToScene(view->mapFromGlobal(QCursor::pos())));

    return false;
}

bool RubberBandInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    QGraphicsView *view = this->view(viewport);

    if (mState != INITIAL || !view->isInteractive())
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    /* Check if there is a focusable item under cursor. */
    struct ItemIsFocusable: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *item) const {
            return item->flags() & QGraphicsItem::ItemIsFocusable;
        }
    };
    QGraphicsItem *focusableItem = item(view, event->pos(), ItemIsFocusable());
    if (focusableItem != NULL)
        return false; /* Let default implementation handle it. */

    mState = PREPAIRING;
    mMousePressPos = event->pos();
    mMousePressScenePos = view->mapToScene(event->pos());

    if (!(event->modifiers() & Qt::ShiftModifier))
        scene()->clearSelection();

    event->accept();
    return false;
}

bool RubberBandInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    QGraphicsView *view = this->view(viewport);

    if (mState == INITIAL || !view->isInteractive())
        return false;

    /* Stop rubber banding if the user has let go of the trigger button (even if we didn't get the release events). */
    if (!(event->buttons() & Qt::LeftButton)) {
        mRubberBand->setViewport(NULL);
        mState = INITIAL;
        return false;
    }

    /* Check for drag distance. */
    if (mState == PREPAIRING) {
        if ((mMousePressPos - event->pos()).manhattanLength() < QApplication::startDragDistance()) {
            return false;
        } else {
            if (event->modifiers() & Qt::ShiftModifier)
                mOriginallySelected = toSet(scene()->selectedItems());
            else
                mOriginallySelected.clear();
            mState = RUBBER_BANDING;
            mRubberBand->setViewport(viewport);
            mRubberBand->setOrigin(mMousePressScenePos);
        }
    }

    /* Update rubber band corner. */
    mRubberBand->setCorner(view->mapToScene(event->pos()));

    /* Update selection. */
    QPainterPath selectionArea;
    selectionArea.addPolygon(view->mapToScene(mRubberBand->rubberBandRect()));
    selectionArea.closeSubpath();

    if (event->modifiers() & Qt::ShiftModifier) {
        /* TODO: this can be implemented more efficiently using intersection of several regions. */
        QSet<QGraphicsItem *> newlySelected = toSet(scene()->items(selectionArea, view->rubberBandSelectionMode(), Qt::DescendingOrder, view->viewportTransform()));

        /* Unselect all items that went out of rubber band. */
        foreach (QGraphicsItem *item, scene()->selectedItems())
            if (!newlySelected.contains(item) && !mOriginallySelected.contains(item))
                item->setSelected(false);

        /* Select all newly selected items. */
        foreach (QGraphicsItem *item, newlySelected)
            item->setSelected(true);
    } else {
        scene()->setSelectionArea(selectionArea, view->rubberBandSelectionMode(), view->viewportTransform());
    }

    event->accept();
    return false; /* Let other instruments receive mouse move events too! */
}

bool RubberBandInstrument::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    QGraphicsView *view = this->view(viewport);

    if (mState == INITIAL || !view->isInteractive())
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    mState = INITIAL;
    mRubberBand->setViewport(NULL);

    event->accept();
    return false;
}

QSet<QGraphicsItem *> RubberBandInstrument::toSet(QList<QGraphicsItem *> items) {
    QSet<QGraphicsItem *> result;
    foreach (QGraphicsItem *item, items)
        result.insert(item);
    return result;
}


