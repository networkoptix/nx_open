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
#include "instrumentmanager.h"

namespace {
    struct ItemIsMouseInteractive: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *item) const {
            return item->acceptedMouseButtons() & Qt::LeftButton;
        }
    };

} // anonymous namespace 

/**
 * Item that implements a rubber band. The downside of implementing rubber band
 * as an item is that <tt>QGraphicsView</tt>'s foreground will be drawn atop of it.
 * 
 * Unfortunately, it is impossible to hook into QGraphicsView's rendering
 * pipeline except by subclassing it, so this is probably the best non-intrusive solution.
 */
class RubberBandItem: public QGraphicsObject {
public:
    RubberBandItem(): 
        QGraphicsObject(NULL),
        m_viewport(NULL),
        m_cacheDirty(true)
    {
        setAcceptedMouseButtons(0);
        setEnabled(false);
    }

    virtual QRectF boundingRect() const override {
        if (m_viewport == NULL)
            return QRectF();

        ensureRects();

        return m_boundingRect;
    }

    /* We can also implement shape(), but most probably there is no point. */

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) override {
        if (widget != m_viewport)
            return; /* Draw it on source viewport only. */

        ensureRects();

        if (m_rubberBandRect.isEmpty())
            return;

        QStyleOptionRubberBand rubberBandOption;
        rubberBandOption.initFrom(widget);
        rubberBandOption.rect = m_rubberBandRect;
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
        m_viewport = viewport;
    }

    /**
     * \returns                         This rubber band item's viewport.
     */
    QWidget *viewport() const {
        return m_viewport;
    }

    /**
     * \param sceneToViewport           Scene-to-viewport transformation for the viewport 
     *                                  that this item will be drawn on.
     */
    void setViewportTransform(const QTransform &sceneToViewport) {
        m_sceneToViewport = sceneToViewport;
        updateRects();
    }

    void setOrigin(const QPointF &origin) {
        m_origin = origin;
        updateRects();
    }

    void setCorner(const QPointF &corner) {
        m_corner = corner;
        updateRects();
    }

    const QTransform &viewportTransform() const {
        return m_sceneToViewport;
    }

    const QPointF &origin() const {
        return m_origin;
    }

    const QPointF &corner() const {
        return m_corner;
    }

    /**
     * \returns                         Current rubber band rectangle in view coordinates.
     */
    const QRect &rubberBandRect() const {
        ensureRects();

        return m_rubberBandRect;
    }

protected:
    void updateRects() {
        prepareGeometryChange();

        m_cacheDirty = true;
    }

    void ensureRects() const {
        if (!m_cacheDirty)
            return;

        /* Create scene-to-viewport transformation. */
        QTransform viewportToScene = m_sceneToViewport.inverted();

        m_rubberBandRect = QRectF(m_sceneToViewport.map(m_origin), m_sceneToViewport.map(m_corner)).normalized().toRect();
        m_boundingRect = viewportToScene.mapRect(QRectF(m_rubberBandRect));
        m_cacheDirty = false;
    }

private:
    /** Viewport that this rubber band will be drawn at. */
    QWidget *m_viewport;

    /** Origin of the rubber band, in scene coordinates. */
    QPointF m_origin;

    /** Second corner of the rubber band, in scene coordinates. */
    QPointF m_corner;

    /** Current viewport-to-scene transformation. */
    QTransform m_sceneToViewport;

    /** Whether stored rectangles need recalculating. */
    mutable bool m_cacheDirty;

    /** Current rubber band rectangle, in viewport coordinates. */
    mutable QRect m_rubberBandRect;

    /** Bounding rect of the current rubber band rectangle, in scene coordinates. */
    mutable QRectF m_boundingRect;
};

RubberBandInstrument::RubberBandInstrument(QObject *parent):
    DragProcessingInstrument(VIEWPORT, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent),
    m_rubberBandZValue(std::numeric_limits<qreal>::max())
{}

RubberBandInstrument::~RubberBandInstrument() {
    ensureUninstalled();
}

void RubberBandInstrument::setRubberBandZValue(qreal rubberBandZValue) {
    m_rubberBandZValue = rubberBandZValue;
    
    if(rubberBand() != NULL)
        rubberBand()->setZValue(m_rubberBandZValue);
}

void RubberBandInstrument::installedNotify() {
    if(rubberBand() != NULL)
        delete rubberBand();

    m_rubberBand = new RubberBandItem();
    rubberBand()->setParent(this); /* Just to feel totally safe. */
    rubberBand()->setZValue(m_rubberBandZValue);
    scene()->addItem(rubberBand());

    connect(scene(), SIGNAL(selectionChanged()), this, SLOT(at_scene_selectionChanged()));

    m_inSelectionChanged = false;
    m_protectSelection = false;

    DragProcessingInstrument::installedNotify();
}

void RubberBandInstrument::aboutToBeUninstalledNotify() {
    DragProcessingInstrument::aboutToBeUninstalledNotify();

    if (scene() != NULL) 
        disconnect(scene(), NULL, this, NULL);

    if(rubberBand() != NULL)
        delete rubberBand();
}

bool RubberBandInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    if(viewport != rubberBand()->viewport())
        return false; /* We are interested only in transformations for the current viewport. */

    QGraphicsView *view = this->view(viewport);
    QTransform sceneToViewport = view->viewportTransform();
    if(qFuzzyCompare(sceneToViewport, rubberBand()->viewportTransform()))
        return false;

    rubberBand()->setViewportTransform(sceneToViewport);

    /* Scene mouse position may have changed as a result of transform change. */
    processor()->paintEvent(viewport, event);

    return false;
}

bool RubberBandInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(!processor()->isWaiting())
        return false;
    
    QGraphicsView *view = this->view(viewport);
    if (!view->isInteractive())
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    /* Check if there is a focusable item under cursor. */
    QGraphicsItem *focusableItem = item(view, event->pos(), ItemIsMouseInteractive());
    if (focusableItem != NULL)
        return false; /* Let default implementation handle it. */

    /* Ok to go. */
    if (!(event->modifiers() & Qt::ShiftModifier))
        scene()->clearSelection();
    
    /* Scene may clear selection after a mouse click. We don't allow it. */
    m_originallySelected = toSet(scene()->selectedItems());
    m_protectSelection = !m_originallySelected.empty();

    processor()->mousePressEvent(viewport, event);
        
    event->accept();
    return false;
}

void RubberBandInstrument::at_scene_selectionChanged() {
    if(!m_protectSelection)
        return;

    if(m_inSelectionChanged)
        return;

    m_inSelectionChanged = true;
    
    foreach(QGraphicsItem *item, m_originallySelected)
        item->setSelected(true);

    m_inSelectionChanged = false;
    m_protectSelection = false;
}

bool RubberBandInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    processor()->mouseMoveEvent(viewport, event);

    event->accept();
    return false;
}

bool RubberBandInstrument::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    processor()->mouseReleaseEvent(viewport, event);

    event->accept();
    return false;
}

void RubberBandInstrument::startDrag() {
    rubberBand()->setViewport(processor()->view()->viewport());
    rubberBand()->setViewportTransform(processor()->view()->viewportTransform());
    rubberBand()->setOrigin(processor()->mousePressScenePos());
}

void RubberBandInstrument::drag() {
    QGraphicsView *view = processor()->view();

    /* Update rubber band corner. */
    rubberBand()->setCorner(processor()->mouseScenePos());

    /* Update selection. */
    QPainterPath selectionArea;
    selectionArea.addPolygon(view->mapToScene(rubberBand()->rubberBandRect()));
    selectionArea.closeSubpath();

    if (processor()->modifiers() & Qt::ShiftModifier) {
        /* TODO: this can be implemented more efficiently using intersection of several regions. */
        QSet<QGraphicsItem *> newlySelected = toSet(scene()->items(selectionArea, view->rubberBandSelectionMode(), Qt::DescendingOrder, view->viewportTransform()));

        /* Unselect all items that went out of rubber band. */
        foreach (QGraphicsItem *item, scene()->selectedItems())
            if (!newlySelected.contains(item) && !m_originallySelected.contains(item))
                item->setSelected(false);

        /* Select all newly selected items. */
        foreach (QGraphicsItem *item, newlySelected)
            item->setSelected(true);
    } else {
        scene()->setSelectionArea(selectionArea, view->rubberBandSelectionMode(), view->viewportTransform());
    }
}

void RubberBandInstrument::finishDrag() {
    rubberBand()->setViewport(NULL);
}

QSet<QGraphicsItem *> RubberBandInstrument::toSet(QList<QGraphicsItem *> items) {
    QSet<QGraphicsItem *> result;
    foreach (QGraphicsItem *item, items)
        result.insert(item);
    return result;
}


