#include "resizing_instrument.h"
#include <QGraphicsWidget>
#include <ui/common/constrained_resizable.h>
#include <ui/common/frame_section_queryable.h>
#include "resize_hover_instrument.h"

namespace {
    struct ItemIsResizableWidget: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *item) const {
            if(!item->isWidget() || !(item->acceptedMouseButtons() & Qt::LeftButton))
                return false;

            QGraphicsWidget *widget = static_cast<QGraphicsWidget *>(item);
            if((widget->windowFlags() & Qt::Window) && (widget->windowFlags() & Qt::WindowTitleHint))
                return true; /* Widget has decorations. */

            if(dynamic_cast<FrameSectionQuearyable *>(widget) != NULL)
                return true; /* No decorations, but still can be queried for frame sections. */

            return false;
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
    base_type(VIEWPORT, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent),
    m_resizeHoverInstrument(new ResizeHoverInstrument(this)),
    m_effectiveDistance(0),
    m_effective(true)
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
    m_startGeometry = widget->geometry();
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

    if(!m_effective)
        return;

    emit resizingStarted(info->view(), m_widget.data(), ResizingInfo(this));
    m_resizingStartedEmitted = true;
}

void ResizingInstrument::dragMove(DragInfo *info) {
    /* Stop resizing if widget was destroyed. */
    if(m_widget.isNull()) {
        dragProcessor()->reset();
        return;
    }

    if(!m_effective)
        return;

    /* Prepare shortcuts. */
    QGraphicsWidget *widget = m_widget.data();
    const QRectF &startGeometry = m_startGeometry;

    /* Calculate new geometry. */
    QLineF delta(widget->mapFromScene(info->mousePressScenePos()), widget->mapFromScene(info->mouseScenePos()));
    QLineF parentDelta(widget->mapToParent(delta.p1()), widget->mapToParent(delta.p2()));
    QLineF parentXDelta(widget->mapToParent(QPointF(delta.p1().x(), 0)), widget->mapToParent(QPointF(delta.p2().x(), 0)));
    QLineF parentYDelta(widget->mapToParent(QPointF(0, delta.p1().y())), widget->mapToParent(QPointF(0, delta.p2().y())));
    
    QRectF newGeometry;
    switch (m_section) {
    case Qt::LeftSection:
        newGeometry = QRectF(
            startGeometry.topLeft() + QPointF(parentXDelta.dx(), parentXDelta.dy()),
            startGeometry.size() - QSizeF(delta.dx(), delta.dy())
        );
        break;
    case Qt::TopLeftSection:
        newGeometry = QRectF(
            startGeometry.topLeft() + QPointF(parentDelta.dx(), parentDelta.dy()),
            startGeometry.size() - QSizeF(delta.dx(), delta.dy())
        );
        break;
    case Qt::TopSection:
        newGeometry = QRectF(
            startGeometry.topLeft() + QPointF(parentYDelta.dx(), parentYDelta.dy()),
            startGeometry.size() - QSizeF(0, delta.dy())
        );
        break;
    case Qt::TopRightSection:
        newGeometry = QRectF(
            startGeometry.topLeft() + QPointF(parentYDelta.dx(), parentYDelta.dy()),
            startGeometry.size() - QSizeF(-delta.dx(), delta.dy())
        );
        break;
    case Qt::RightSection:
        newGeometry = QRectF(
            startGeometry.topLeft(),
            startGeometry.size() + QSizeF(delta.dx(), 0)
        );
        break;
    case Qt::BottomRightSection:
        newGeometry = QRectF(
            startGeometry.topLeft(),
            startGeometry.size() + QSizeF(delta.dx(), delta.dy())
        );
        break;
    case Qt::BottomSection:
        newGeometry = QRectF(
            startGeometry.topLeft(),
            startGeometry.size() + QSizeF(0, delta.dy())
        );
        break;
    case Qt::BottomLeftSection:
        newGeometry = QRectF(
            startGeometry.topLeft() + QPointF(parentXDelta.dx(), parentXDelta.dy()),
            startGeometry.size() - QSizeF(delta.dx(), -delta.dy())
        );
        break;
    default:
        break;
    }

    /* Adjust for size hints. */
    QSizeF minSize = widget->effectiveSizeHint(Qt::MinimumSize);
    QSizeF maxSize = widget->effectiveSizeHint(Qt::MaximumSize);
    QSizeF size = QSizeF(
        qBound(minSize.width(), newGeometry.width(), maxSize.width()),
        qBound(minSize.height(), newGeometry.height(), maxSize.height())
    );
    /* We don't handle heightForWidth. */

    if(m_resizable != NULL)
        size = m_resizable->constrainedSize(size);

    newGeometry = resizeRect(startGeometry, size, m_section);
    widget->setGeometry(newGeometry);

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
