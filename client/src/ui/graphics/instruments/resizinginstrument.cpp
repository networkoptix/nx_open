#include "resizinginstrument.h"
#include <QGraphicsWidget>
#include <ui/common/constrained_resizable.h>

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

ResizingInstrument::ResizingInstrument(QObject *parent):
    DragProcessingInstrument(VIEWPORT, makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease, QEvent::Paint), parent)
{}

ResizingInstrument::~ResizingInstrument() {
    ensureUninstalled();
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
    QPointF scenePos = view->mapToScene(event->pos());
    QPointF itemPos = widget->mapFromScene(scenePos);
    Qt::WindowFrameSection section = open(widget)->getWindowFrameSectionAt(itemPos);
    if(!isResizeGrip(section))
        return false;

    /* Ok to go. */
    m_startGeometry = widget->geometry();
    m_section = section;
    m_widget = widget;
    m_resizable = dynamic_cast<ConstrainedResizable *>(widget);

    dragProcessor()->mousePressEvent(viewport, event);

    event->accept();
    return true; /* We don't want default event handlers to kick in. */
}

bool ResizingInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    dragProcessor()->mouseMoveEvent(viewport, event);
    
    event->accept();
    return false;
}

bool ResizingInstrument::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    dragProcessor()->mouseReleaseEvent(viewport, event);

    event->accept();
    return false;
}

bool ResizingInstrument::paintEvent(QWidget *viewport, QPaintEvent *event) {
    dragProcessor()->paintEvent(viewport, event);

    return false;
}

void ResizingInstrument::startDragProcess(DragInfo *info) {
    emit resizingProcessStarted(info->view(), m_widget.data());
}

void ResizingInstrument::startDrag(DragInfo *info) {
    m_resizingStartedEmitted = false;

    if(m_widget.isNull()) {
        /** Whoops, already destroyed. */
        dragProcessor()->reset();
        return;
    }

    emit resizingStarted(info->view(), m_widget.data());
    m_resizingStartedEmitted = true;
}

void ResizingInstrument::dragMove(DragInfo *info) {
    /* Stop resizing if widget was destroyed. */
    if(m_widget.isNull()) {
        dragProcessor()->reset();
        return;
    }

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

    switch (m_section) {
    case Qt::LeftSection:
        newGeometry = QRectF(
            startGeometry.right() - size.width(), 
            startGeometry.top(),
            size.width(),
            startGeometry.height()
        );
        break;
    case Qt::TopLeftSection:
        newGeometry = QRectF(
            startGeometry.right() - size.width(), 
            startGeometry.bottom() - size.height(),
            size.width(), 
            size.height()
        );
        break;
    case Qt::TopSection:
        newGeometry = QRectF(
            startGeometry.left(), 
            startGeometry.bottom() - size.height(),
            startGeometry.width(), 
            size.height()
        );
        break;
    case Qt::TopRightSection:
        newGeometry.setTop(startGeometry.bottom() - size.height());
        newGeometry.setWidth(size.width());
        break;
    case Qt::RightSection:
        newGeometry.setWidth(size.width());
        break;
    case Qt::BottomRightSection:
        newGeometry.setWidth(size.width());
        newGeometry.setHeight(size.height());
        break;
    case Qt::BottomSection:
        newGeometry.setHeight(size.height());
        break;
    case Qt::BottomLeftSection:
        newGeometry = QRectF(
            startGeometry.right() - size.width(), 
            startGeometry.top(),
            size.width(), 
            size.height()
        );
        break;
    default:
        break;
    }

    widget->setGeometry(newGeometry);
}

void ResizingInstrument::finishDrag(DragInfo *info) {
    if(m_resizingStartedEmitted)
        emit resizingFinished(info->view(), m_widget.data());

    m_widget.clear();
    m_resizable = NULL;
}

void ResizingInstrument::finishDragProcess(DragInfo *info) {
    emit resizingProcessFinished(info->view(), m_widget.data());
}
