#include "motion_selection_instrument.h"

#include <cassert>

#include <QtWidgets/QGraphicsObject>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>

#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/warnings.h>
#include <utils/common/variant.h>

#include <core/resource/resource.h>

#include <ui/graphics/items/resource/media_resource_widget.h>

#include "selection_item.h"

namespace {

    struct BlocksMotionSelection {
    public:
        BlocksMotionSelection(bool *blockMotionClearing): m_blockMotionClearing(blockMotionClearing) {
            *m_blockMotionClearing = false;
        }

        bool operator()(QGraphicsItem *item) const {
            if(!(item->acceptedMouseButtons() & Qt::LeftButton))
                return false; /* Skip to next item. */
                
            if(item->isWidget() && dynamic_cast<QnMediaResourceWidget *>(item))
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


MotionSelectionInstrument::MotionSelectionInstrument(QObject *parent):
    base_type(Viewport, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent),
    m_isClick(false),
    m_selectionModifiers(0),
    m_multiSelectionModifiers(Qt::ControlModifier)
{
    /* Default-initialize pen & brush from temporary selection item. */
    QScopedPointer<SelectionItem> item(new SelectionItem());
    m_pen = item->pen();
    m_brush = item->brush();
}

MotionSelectionInstrument::~MotionSelectionInstrument() {
    ensureUninstalled();
}

void MotionSelectionInstrument::setPen(const QPen &pen) {
    m_pen = pen;

    if(selectionItem())
        selectionItem()->setPen(pen);
}

QPen MotionSelectionInstrument::pen() const {
    return m_pen;
}

void MotionSelectionInstrument::setBrush(const QBrush &brush) {
    m_brush = brush;

    if(selectionItem())
        selectionItem()->setBrush(brush);
}

QBrush MotionSelectionInstrument::brush() const {
    return m_brush;
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

    m_selectionItem = new SelectionItem();
    selectionItem()->setVisible(false);
    selectionItem()->setPen(m_pen);
    selectionItem()->setBrush(m_brush);

    if(scene() != NULL)
        scene()->addItem(selectionItem());
}

Qt::KeyboardModifiers MotionSelectionInstrument::selectionModifiers(QnMediaResourceWidget *target) const {
    if(!target)
        return m_selectionModifiers;

    return static_cast<Qt::KeyboardModifiers>(qvariant_cast<int>(target->property(Qn::MotionSelectionModifiers), m_selectionModifiers));
}

bool MotionSelectionInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(event->button() != Qt::LeftButton)
        return false;

    QGraphicsView *view = this->view(viewport);
    QnMediaResourceWidget *target = dynamic_cast<QnMediaResourceWidget *>(this->item(view, event->pos(), BlocksMotionSelection(&m_clearingBlocked)));
    if(!target)
        return false;

    if(!target->resource()->toResource()->hasFlags(Qn::motion))
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
    m_gridRect = QRect();

    if(target() == NULL) {
        /* Whoops, already destroyed. */
        dragProcessor()->reset();
        return;
    }

    if ((info->modifiers() & m_multiSelectionModifiers) != m_multiSelectionModifiers)
        emit motionRegionCleared(info->view(), target());

    ensureSelectionItem();
    selectionItem()->setParentItem(target());
    selectionItem()->setViewport(info->view()->viewport());
    selectionItem()->setVisible(true);
    /* Everything else will be initialized in the first call to drag(). */

    emit selectionStarted(info->view(), target());
    m_selectionStartedEmitted = true;
}

void MotionSelectionInstrument::dragMove(DragInfo *info) {
    if(target() == NULL) {
        dragProcessor()->reset();
        return;
    }
    ensureSelectionItem();

    QPointF mouseOrigin = target()->mapFromScene(info->mousePressScenePos());
    QPointF mouseCorner = target()->mapFromScene(info->mouseScenePos());

    QPointF gridStep = target()->mapFromMotionGrid(QPoint(1, 1)) - target()->mapFromMotionGrid(QPoint(0, 0));
    QPointF mouseDelta = mouseOrigin - mouseCorner;
    if(qAbs(mouseDelta.x()) < qAbs(gridStep.x()) / 2 && qAbs(mouseDelta.y()) < qAbs(gridStep.y()) / 2) {
        selectionItem()->setRect(QPointF(0, 0), QPointF(0, 0)); /* Ignore small deltas. */
        m_gridRect = QRect();
    } else {
        QPoint gridOrigin = target()->mapToMotionGrid(mouseOrigin);
        QPoint gridCorner = target()->mapToMotionGrid(mouseCorner) + QPoint(1, 1); 

        if (gridCorner.x() <= gridOrigin.x()) {
            gridCorner -= QPoint(1, 0);
            gridOrigin += QPoint(1, 0);
        }

        if (gridCorner.y() <= gridOrigin.y()) {
            gridCorner -= QPoint(0, 1);
            gridOrigin += QPoint(0, 1);
        }

        QPointF origin = target()->mapFromMotionGrid(gridOrigin);
        QPointF corner = target()->mapFromMotionGrid(gridCorner);
        selectionItem()->setRect(origin, corner);

        m_gridRect = QRect(gridOrigin, gridCorner).normalized().adjusted(0, 0, -1, -1);
    }
}

void MotionSelectionInstrument::finishDrag(DragInfo *info) {
    ensureSelectionItem();
    if(target() != NULL) {
        emit motionRegionSelected(
            info->view(), 
            target(), 
            m_gridRect
        );
    }

    selectionItem()->setVisible(false);
    selectionItem()->setParentItem(NULL);

    if(m_selectionStartedEmitted)
        emit selectionFinished(info->view(), target());
}

void MotionSelectionInstrument::finishDragProcess(DragInfo *info) {
#if 0
    if (m_isClick && !m_clearingBlocked && target()) {
        Qt::KeyboardModifiers selectionModifiers = this->selectionModifiers(target());
        if((info->modifiers() & selectionModifiers) == selectionModifiers && (info->modifiers() & m_multiSelectionModifiers) != m_multiSelectionModifiers) {
            emit motionRegionCleared(info->view(), target());
            m_isClick = false;
        }
    }
#endif

    emit selectionProcessFinished(info->view(), target());
}

SelectionItem *MotionSelectionInstrument::selectionItem() const {
    return m_selectionItem.data();
}

QnMediaResourceWidget *MotionSelectionInstrument::target() const {
    return m_target.data();
}

