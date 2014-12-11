#include "resizing_instrument.h"
#include <QtWidgets/QGraphicsWidget>
#include <ui/common/constrained_resizable.h>
#include <ui/common/frame_section_queryable.h>
#include "resize_hover_instrument.h"

namespace {
    struct ItemIsResizableWidget: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *item) const {
            return item->isWidget() && (item->acceptedMouseButtons() & Qt::LeftButton);
        }
    };

    class GraphicsWidget: public QGraphicsWidget {
    public:
        friend class ResizingInstrument;

        Qt::WindowFrameSection getWindowFrameSectionAt(const QPointF &pos) const {
            return this->windowFrameSectionAt(pos);
        }
    };

    GraphicsWidget *open(QGraphicsWidget *widget) {
        return static_cast<GraphicsWidget *>(widget);
    }

    /**
     * \param section                   Window frame section.
     * \returns                         Whether the given section can be used as a grip for resizing a window.
     */
    bool isResizeGrip(Qt::WindowFrameSection section) {
        return section != Qt::NoSection && section != Qt::TitleBarArea; 
    }

} // anonymous namespace


// -------------------------------------------------------------------------- //
// ResizingInfo
// -------------------------------------------------------------------------- //
Qt::WindowFrameSection ResizingInfo::frameSection() const {
    return m_instrument->m_section;
}

QPoint ResizingInfo::mouseScreenPos() const {
    return m_info->mouseScreenPos();
}

QPoint ResizingInfo::mouseViewportPos() const {
    return m_info->mouseViewportPos();
}

QPointF ResizingInfo::mouseScenePos() const {
    return m_info->mouseScenePos();
}


// -------------------------------------------------------------------------- //
// ResizingInstrument
// -------------------------------------------------------------------------- //
ResizingInstrument::ResizingInstrument(QObject *parent):
    base_type(Viewport, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent),
    m_resizeHoverInstrument(new ResizeHoverInstrument(this)),
    m_effectRadius(0.0)
{}

ResizingInstrument::~ResizingInstrument() {
    ensureUninstalled();
}

Instrument *ResizingInstrument::resizeHoverInstrument() const {
    return m_resizeHoverInstrument;
}

void ResizingInstrument::rehandle() {
    dragProcessor()->redrag();
}

void ResizingInstrument::enabledNotify() {
    m_resizeHoverInstrument->recursiveEnable();

    base_type::enabledNotify();
}

void ResizingInstrument::aboutToBeDisabledNotify() {
    base_type::aboutToBeDisabledNotify();

    m_resizeHoverInstrument->recursiveDisable();
}

bool ResizingInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    if(!dragProcessor()->isWaiting())
        return false;
    
    QGraphicsView *view = this->view(viewport);
    if (!view->isInteractive())
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    /* Find the item to resize. */
    QGraphicsWidget *widget = static_cast<QGraphicsWidget *>(item(view, event->pos(), ItemIsResizableWidget()));
    if (widget == NULL || !satisfiesItemConditions(widget))
        return false;

    /* Check frame section. */
    FrameSectionQueryable *queryable = dynamic_cast<FrameSectionQueryable *>(widget);
    if(!queryable && !((widget->windowFlags() & Qt::Window) && (widget->windowFlags() & Qt::WindowTitleHint)))
        return false; /* Has no decorations and not queryable for frame sections. */

    QPointF itemPos = widget->mapFromScene(view->mapToScene(event->pos()));
    Qt::WindowFrameSection section;
    if(queryable == NULL) {
        section = open(widget)->getWindowFrameSectionAt(itemPos);
    } else {
        QRectF effectRect = widget->mapRectFromScene(mapRectToScene(view, QRectF(0, 0, m_effectRadius, m_effectRadius)));
        qreal effectRadius = qMax(effectRect.width(), effectRect.height());
        section = queryable->windowFrameSectionAt(QRectF(itemPos - QPointF(effectRadius, effectRadius), QSizeF(2 * effectRadius, 2 * effectRadius)));
    }
    if(section == Qt::NoSection)
        return false;

    /* Ok to go. */
    m_startSize = widget->size();
    m_section = section;
    m_widget = widget;
    m_constrained = dynamic_cast<ConstrainedGeometrically *>(widget);
    if(section == Qt::TitleBarArea) {
        m_startPinPoint = cwiseDiv(itemPos, m_startSize);

        /* Make sure we don't get NaNs in startPinPoint. */
        if(qFuzzyIsNull(m_startSize.width()))
            m_startPinPoint.setX(0.0);
        if(qFuzzyIsNull(m_startSize.height()))
            m_startPinPoint.setY(0.0);
    } else {
        m_startPinPoint = widget->mapToParent(Qn::calculatePinPoint(widget->rect(), section));
    }

    dragProcessor()->mousePressEvent(viewport, event);

    event->accept();
    return false;
}

bool ResizingInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    QGraphicsView *view = this->view(viewport);

    QRectF effectRect = mapRectToScene(view, QRectF(0, 0, m_effectRadius, m_effectRadius));
    qreal effectRadius = qMax(effectRect.width(), effectRect.height());
    m_resizeHoverInstrument->setEffectRadius(effectRadius);

    return base_type::paintEvent(viewport, event);
}

void ResizingInstrument::startDragProcess(DragInfo *info) {
    ResizingInfo resizingInfo(info, this);
    emit resizingProcessStarted(info->view(), m_widget.data(), &resizingInfo);
}

void ResizingInstrument::startDrag(DragInfo *info) {
    m_resizingStartedEmitted = false;

    if(m_widget.isNull()) {
        /* Whoops, already destroyed. */
        dragProcessor()->reset();
        return;
    }

    ResizingInfo resizingInfo(info, this);
    emit resizingStarted(info->view(), m_widget.data(), &resizingInfo);
    m_resizingStartedEmitted = true;
}

void ResizingInstrument::dragMove(DragInfo *info) {
    /* Stop resizing if widget was destroyed. */
    QGraphicsWidget *widget = m_widget.data();
    if(!widget) {
        dragProcessor()->reset();
        return;
    }

    /* Calculate new size. */
    QSizeF newSize;
    if(m_section == Qt::TitleBarArea) {
        newSize = widget->size();
    } else {
        newSize = m_startSize + Qn::calculateSizeDelta(
            widget->mapFromScene(info->mouseScenePos()) - widget->mapFromScene(info->mousePressScenePos()), 
            m_section
        );
        QSizeF minSize = widget->effectiveSizeHint(Qt::MinimumSize);
        QSizeF maxSize = widget->effectiveSizeHint(Qt::MaximumSize);
        newSize = QSizeF(
            qBound(minSize.width(), newSize.width(), maxSize.width()),
            qBound(minSize.height(), newSize.height(), maxSize.height())
        );
        /* We don't handle heightForWidth. */
    }

    /* Calculate new position. */
    QPointF newPos;
    if(m_section == Qt::TitleBarArea) {
        QPointF itemPos = widget->mapFromScene(info->mouseScenePos());
        newPos = widget->mapToParent(itemPos - QnGeometry::cwiseMul(m_startPinPoint, newSize));
    } else {
        newPos = widget->pos() + m_startPinPoint - widget->mapToParent(Qn::calculatePinPoint(QRectF(QPointF(0.0, 0.0), newSize), m_section));
    }

    if(m_constrained != NULL) {
        QRectF newRect = m_constrained->constrainedGeometry(QRectF(newPos, newSize), m_section);
        newPos = newRect.topLeft();
        newSize = newRect.size();
    }

    /* Change size. */
    widget->resize(newSize);
    newSize = widget->size();

    /* Recalculate new position. 
     * Note that changes in size may change item's transformation => 
     * we have to calculate position after setting the size. */
    // TODO: totally evil copypasta
    if(m_section != Qt::TitleBarArea)
        newPos = widget->pos() + m_startPinPoint - widget->mapToParent(Qn::calculatePinPoint(QRectF(QPointF(0.0, 0.0), newSize), m_section));
    
    /* Change position. */
    widget->setPos(newPos);

    ResizingInfo resizingInfo(info, this);
    emit resizing(info->view(), widget, &resizingInfo);
}

void ResizingInstrument::finishDrag(DragInfo *info) {
    if(m_resizingStartedEmitted) {
        ResizingInfo resizingInfo(info, this);
        emit resizingFinished(info->view(), m_widget.data(), &resizingInfo);
    }

    m_widget.clear();
    m_constrained = NULL;
}

void ResizingInstrument::finishDragProcess(DragInfo *info) {
    ResizingInfo resizingInfo(info, this);
    emit resizingProcessFinished(info->view(), m_widget.data(), &resizingInfo);
}
