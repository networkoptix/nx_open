#include "motion_selection_instrument.h"

#include <cassert>

#include <QtGui/QGraphicsObject>
#include <QtGui/QMouseEvent>
#include <QtGui/QApplication>
#include <QtGui/QStyle>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/warnings.h>
#include <utils/common/variant.h>

#include <ui/graphics/items/resource_widget.h>

namespace {

    struct BlocksMotionSelection {
    public:
        BlocksMotionSelection(bool *blockMotionClearing): m_blockMotionClearing(blockMotionClearing) {
            *m_blockMotionClearing = false;
        }

        bool operator()(QGraphicsItem *item) const {
            if(!(item->acceptedMouseButtons() & Qt::LeftButton))
                return false; /* Skip to next item. */
                
            if(item->isWidget() && dynamic_cast<QnResourceWidget *>(item))
                return true;

            if(item->toGraphicsObject() && item->toGraphicsObject()->property(Qn::NoBlockMotionSelection).toBool()) {
                *m_blockMotionClearing = true; 
                return false;
            }

            return true;
        }

    private:
        bool *m_blockMotionClearing;
    };

} // anonymous namespace


class MotionSelectionItem: public QGraphicsObject {
public:
    MotionSelectionItem(): 
        QGraphicsObject(NULL),
        m_viewport(NULL)
    {
        setAcceptedMouseButtons(0);

        /* Don't disable this item here or it will swallow mouse wheel events. */
    }

    virtual QRectF boundingRect() const override {
        return QRectF(m_origin, m_corner).normalized();
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *widget) override {
        if (widget != m_viewport)
            return; /* Draw it on source viewport only. */

        QnScopedPainterPenRollback penRollback(painter, m_colors[MotionSelectionInstrument::Border]);
        QnScopedPainterBrushRollback brushRollback(painter, m_colors[MotionSelectionInstrument::Base]);
        painter->drawRect(boundingRect());
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

    void setOrigin(const QPointF &origin) {
        prepareGeometryChange();
        m_origin = origin;
    }

    void setCorner(const QPointF &corner) {
        prepareGeometryChange();
        m_corner = corner;
    }

    void setGridOrigin(const QPoint &grid_origin) {
        m_grid_origin = grid_origin;
    }

    void setGridCorner(const QPoint &grid_corner) {
        m_grid_corner = grid_corner;
    }

    const QPointF &origin() const {
        return m_origin;
    }

    const QPointF &corner() const {
        return m_corner;
    }

    const QPoint &grid_origin() const {
        return m_grid_origin;
    }

    const QPoint &grid_corner() const {
        return m_grid_corner;
    }

    void setColor(MotionSelectionInstrument::ColorRole role, const QColor &color) {
        m_colors[role] = color;

        update();
    }

private:
    /** Viewport that this selection item will be drawn at. */
    QWidget *m_viewport;

    /** Origin of the selection item, in parent coordinates. */
    QPointF m_origin;

    /** Second corner of the selection item, in parent coordinates. */
    QPointF m_corner;

    /** Origin of the selection item, in grid coordinates. */
    QPoint m_grid_origin;

    /** Second corner of the selection item, in grid coordinates. */
    QPoint m_grid_corner;

    /** Colors for drawing the selection rect. */
    QColor m_colors[MotionSelectionInstrument::RoleCount];
};

MotionSelectionInstrument::MotionSelectionInstrument(QObject *parent):
    base_type(Viewport, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent),
    m_isClick(false),
    m_selectionModifiers(0),
    m_multiSelectionModifiers(Qt::ControlModifier)
{
    /* Initialize colors with some sensible defaults. Calculations are taken from XP style. */
    QPalette palette = QApplication::style()->standardPalette();
    QColor highlight = palette.color(QPalette::Active, QPalette::Highlight);
    m_colors[Border] = highlight.darker(120);
    m_colors[Base] = QColor(
        qMin(highlight.red() / 2 + 110, 255),
        qMin(highlight.green() / 2 + 110, 255),
        qMin(highlight.blue() / 2 + 110, 255),
        127
    );
}

MotionSelectionInstrument::~MotionSelectionInstrument() {
    ensureUninstalled();
}

void MotionSelectionInstrument::setColor(ColorRole role, const QColor &color) {
    if(role < 0 || role >= RoleCount) {
        qnWarning("Invalid color role '%1'.", static_cast<int>(role));
        return;
    }

    m_colors[role] = color;

    if(selectionItem())
        selectionItem()->setColor(role, color);
}

QColor MotionSelectionInstrument::color(ColorRole role) const {
    if(role < 0 || role >= RoleCount) {
        qnWarning("Invalid color role '%1'.", static_cast<int>(role));
        return QColor();
    }

    return m_colors[role];
}

void MotionSelectionInstrument::setSelectionModifiers(Qt::KeyboardModifiers selectionModifiers) {
    m_selectionModifiers = selectionModifiers;
}

Qt::KeyboardModifiers MotionSelectionInstrument::selectionModifiers() const {
    return m_selectionModifiers;
}

void MotionSelectionInstrument::setMultiSelectionModifiers(Qt::KeyboardModifiers multiSelectionModifiers) {
    m_multiSelectionModifiers = multiSelectionModifiers;
}

Qt::KeyboardModifiers MotionSelectionInstrument::multiSelectionModifiers() const {
    return m_multiSelectionModifiers;
}

void MotionSelectionInstrument::installedNotify() {
    assert(selectionItem() == NULL);

    ensureSelectionItem();

    base_type::installedNotify();
}

void MotionSelectionInstrument::aboutToBeDisabledNotify() {
    m_isClick = false;

    base_type::aboutToBeDisabledNotify();
}

void MotionSelectionInstrument::aboutToBeUninstalledNotify() {
    base_type::aboutToBeUninstalledNotify();

    if(selectionItem() != NULL)
        delete selectionItem();
}

void MotionSelectionInstrument::ensureSelectionItem() {
    if(selectionItem() != NULL)
        return;

    m_selectionItem = new MotionSelectionItem();
    selectionItem()->setVisible(false);
    selectionItem()->setColor(Base,     m_colors[Base]);
    selectionItem()->setColor(Border,   m_colors[Border]);
    if(scene() != NULL)
        scene()->addItem(selectionItem());
}

Qt::KeyboardModifiers MotionSelectionInstrument::selectionModifiers(QnResourceWidget *target) const {
    if(!target)
        return m_selectionModifiers;

    return qvariant_cast<int>(target->property(Qn::MotionSelectionModifiers), m_selectionModifiers);
}

bool MotionSelectionInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    QGraphicsView *view = this->view(viewport);
    QnResourceWidget *target = dynamic_cast<QnResourceWidget *>(this->item(view, event->pos(), BlocksMotionSelection(&m_clearingBlocked)));
    if(!target)
        return false;

    Qt::KeyboardModifiers selectionModifiers = this->selectionModifiers(target);
    if((event->modifiers() & selectionModifiers) != selectionModifiers)
        return false;

    m_target = target;
    
    dragProcessor()->mousePressEvent(viewport, event);
    
    event->accept();
    return false;
}

bool MotionSelectionInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    if(target() == NULL) {
        dragProcessor()->reset();
        return false;
    }

    return base_type::paintEvent(viewport, event);
}

void MotionSelectionInstrument::startDragProcess(DragInfo *info) {
    m_isClick = true;
    emit selectionProcessStarted(info->view(), target());
}

void MotionSelectionInstrument::startDrag(DragInfo *info) {
    m_isClick = false;
    m_selectionStartedEmitted = false;

    if(target() == NULL) {
        /* Whoops, already destroyed. */
        dragProcessor()->reset();
        return;
    }

    if ((info->modifiers() & m_multiSelectionModifiers) != m_multiSelectionModifiers)
        emit motionRegionCleared(info->view(), target());

    ensureSelectionItem();
    selectionItem()->setParentItem(target());
    QPointF itemPos = target()->mapFromScene(info->mousePressScenePos());
    QPoint gridPos = target()->mapToMotionGrid(itemPos);
    selectionItem()->setOrigin(target()->mapFromMotionGrid(gridPos));
    selectionItem()->setGridOrigin(gridPos);
    selectionItem()->setViewport(info->view()->viewport());
    selectionItem()->setVisible(true);

    emit selectionStarted(info->view(), target());
    m_selectionStartedEmitted = true;
}

void MotionSelectionInstrument::dragMove(DragInfo *info) {
    if(target() == NULL) {
        dragProcessor()->reset();
        return;
    }
    ensureSelectionItem();
    QPoint gridOrigin = target()->mapToMotionGrid(target()->mapFromScene(info->mousePressScenePos()));
    QPoint gridCorner = target()->mapToMotionGrid(target()->mapFromScene(info->mouseScenePos())) + QPoint(1, 1);
    
    if (gridCorner.x() <= gridOrigin.x()){
        gridCorner -= QPoint(1, 0);
        gridOrigin += QPoint(1, 0);
    }

    if (gridCorner.y() <= gridOrigin.y()){
        gridCorner -= QPoint(0, 1);
        gridOrigin += QPoint(0, 1);
    }

    selectionItem()->setGridOrigin(gridOrigin);
    selectionItem()->setGridCorner(gridCorner);
    selectionItem()->setOrigin(target()->mapFromMotionGrid(gridOrigin));
    selectionItem()->setCorner(target()->mapFromMotionGrid(gridCorner));

    qnDebug("mouse %1 %2", info->mouseScenePos().x(), info->mouseScenePos().y());
    qnDebug("origin %1 %2", gridOrigin.x(), gridOrigin.y());
    qnDebug("corner %1 %2", gridCorner.x(), gridCorner.y());
}

void MotionSelectionInstrument::finishDrag(DragInfo *info) {
    ensureSelectionItem();
    if(target() != NULL) {
        /* Qt handles QRect borders in totally inhuman way, so we have to do everything by hand. */
        /*QPoint o = target()->mapToMotionGrid(selectionItem()->origin());
        QPoint c = target()->mapToMotionGrid(selectionItem()->corner());*/

        QPoint o = selectionItem()->grid_origin();
        QPoint c = selectionItem()->grid_corner();

        QRect new_selection = selectionItem()->boundingRect().toRect();

        qnDebug("finish origin %1 %2", selectionItem()->origin().x(), selectionItem()->origin().y());
        qnDebug("finish corner %1 %2", selectionItem()->corner().x(), selectionItem()->corner().y());
        qnDebug("o %1 %2", o.x(), o.y());
        qnDebug("c %1 %2", c.x(), c.y());
        QRect old_selection = QRect(
            QPoint(qMin(o.x(), c.x()), qMin(o.y(), c.y())),
            QSize(qAbs(o.x() - c.x()), qAbs(o.y() - c.y()))
            );

        emit motionRegionSelected(
            info->view(), 
            target(), 
            /*QRect(
                QPoint(qMin(o.x(), c.x()), qMin(o.y(), c.y())),
                QSize(qAbs(o.x() - c.x()), qAbs(o.y() - c.y()))
            )*/ old_selection
        );
    }

    selectionItem()->setVisible(false);
    selectionItem()->setParentItem(NULL);

    if(m_selectionStartedEmitted)
        emit selectionFinished(info->view(), target());
}

void MotionSelectionInstrument::finishDragProcess(DragInfo *info) {
    if (m_isClick && !m_clearingBlocked && target()) {
        Qt::KeyboardModifiers selectionModifiers = this->selectionModifiers(target());
        if((info->modifiers() & selectionModifiers) == selectionModifiers && (info->modifiers() & m_multiSelectionModifiers) != m_multiSelectionModifiers) {
            emit motionRegionCleared(info->view(), target());
            m_isClick = false;
        }
    }

    emit selectionProcessFinished(info->view(), target());
}
