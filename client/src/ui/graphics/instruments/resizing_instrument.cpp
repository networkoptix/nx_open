#include "resizing_instrument.h"
#include <QGraphicsWidget>
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

Qt::WindowFrameSection ResizingInfo::frameSection() const {
    return m_instrument->m_section;
}

ResizingInstrument::ResizingInstrument(QObject *parent):
    base_type(Viewport, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent),
    m_resizeHoverInstrument(new ResizeHoverInstrument(this)),
    m_effectiveDistance(0)
{}

ResizingInstrument::~ResizingInstrument() {
    ensureUninstalled();
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
    if (widget == NULL)
        return false;

    /* Check frame section. */
    FrameSectionQuearyable *queryable = dynamic_cast<FrameSectionQuearyable *>(widget);
    if(!queryable && !((widget->windowFlags() & Qt::Window) && (widget->windowFlags() & Qt::WindowTitleHint)))
        return false; /* Has no decorations and not queryable for frame sections. */

    QPointF itemPos = widget->mapFromScene(view->mapToScene(event->pos()));
    Qt::WindowFrameSection section;
    if(queryable == NULL) {
        section = open(widget)->getWindowFrameSectionAt(itemPos);
    } else {
        QRectF effectiveRect = widget->mapRectFromScene(mapRectToScene(view, QRect(0, 0, m_effectiveDistance, m_effectiveDistance)));
        qreal effectiveDistance = qMax(effectiveRect.width(), effectiveRect.height());
        section = queryable->windowFrameSectionAt(QRectF(itemPos - QPointF(effectiveDistance, effectiveDistance), QSizeF(2 * effectiveDistance, 2 * effectiveDistance)));
    }
    if(!isResizeGrip(section))
        return false;

    /* Ok to go. */
    m_startSize = widget->size();
    m_startPinPoint = widget->mapToParent(Qn::calculatePinPoint(widget->rect(), section));
    m_section = section;
    m_widget = widget;
    m_resizable = dynamic_cast<ConstrainedResizable *>(widget);

    dragProcessor()->mousePressEvent(viewport, event);

    event->accept();
    return false;
}

bool ResizingInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    QGraphicsView *view = this->view(viewport);

    QRectF effectiveRect = mapRectToScene(view, QRect(0, 0, m_effectiveDistance, m_effectiveDistance));
    qreal effectiveDistance = qMax(effectiveRect.width(), effectiveRect.height());
    m_resizeHoverInstrument->setEffectiveDistance(effectiveDistance);

    return base_type::paintEvent(viewport, event);
}

void ResizingInstrument::startDragProcess(DragInfo *info) {
    emit resizingProcessStarted(info->view(), m_widget.data(), ResizingInfo(this));
}

void ResizingInstrument::startDrag(DragInfo *info) {
    m_resizingStartedEmitted = false;

    if(m_widget.isNull()) {
        /* Whoops, already destroyed. */
        dragProcessor()->reset();
        return;
    }

    emit resizingStarted(info->view(), m_widget.data(), ResizingInfo(this));
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
    QSizeF newSize = m_startSize + Qn::calculateSizeDelta(
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

    if(m_resizable != NULL)
        newSize = m_resizable->constrainedSize(newSize);

    /* Change size & position. */
    widget->resize(newSize);
    widget->setPos(widget->pos() + m_startPinPoint - widget->mapToParent(Qn::calculatePinPoint(widget->rect(), m_section)));

    emit resizing(info->view(), widget, ResizingInfo(this));
}

void ResizingInstrument::finishDrag(DragInfo *info) {
    if(m_resizingStartedEmitted)
        emit resizingFinished(info->view(), m_widget.data(), ResizingInfo(this));

    m_widget.clear();
    m_resizable = NULL;
}

void ResizingInstrument::finishDragProcess(DragInfo *info) {
    emit resizingProcessFinished(info->view(), m_widget.data(), ResizingInfo(this));
}
