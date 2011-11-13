#include "resizinginstrument.h"
#include "constrainedresizable.h"

namespace {
    struct ItemIsWidget: public std::unary_function<QGraphicsItem *, bool> {
        bool operator()(QGraphicsItem *item) const {
            return item->isWidget();
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
    Instrument(makeSet(), makeSet(), makeSet(QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::MouseButtonRelease), makeSet(), parent),
    m_state(INITIAL)
{}

ResizingInstrument::~ResizingInstrument() {
    ensureUninstalled();
}

void ResizingInstrument::installedNotify() {
    m_state = INITIAL;
}

void ResizingInstrument::aboutToBeUninstalledNotify() {
    
}

void ResizingInstrument::aboutToBeDisabledNotify() {

}

bool ResizingInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event) {
    QGraphicsView *view = this->view(viewport);

    if (m_state != INITIAL || !view->isInteractive())
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    /* Find the item to resize. */
    QGraphicsWidget *widget = static_cast<QGraphicsWidget *>(item(view, event->pos(), ItemIsWidget()));

    qDebug("WIDGET = %x", widget);

    if (widget == NULL)
        return false;

    /* Check frame section. */
    QPointF scenePos = view->mapToScene(event->pos());
    QPointF itemPos = widget->mapFromScene(scenePos);
    Qt::WindowFrameSection section = open(widget)->getWindowFrameSectionAt(itemPos);
    if(!isResizeGrip(section))
        return false;

    m_state = PREPAIRING;
    m_mousePressPos = event->pos();
    m_mousePressScenePos = scenePos;
    m_lastMouseScenePos = scenePos;
    m_startGeometry = widget->geometry();
    m_section = section;
    m_view = view;
    m_widget = widget;
    m_resizable = dynamic_cast<ConstrainedResizable *>(widget);

    emit resizingProcessStarted(view, widget);

    event->accept();
    return true; /* We don't want default event handlers to kick in. */
}

bool ResizingInstrument::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {
    QGraphicsView *view = this->view(viewport);

    if (m_state == INITIAL || !view->isInteractive())
        return false;

    /* Stop resizing if the user has let go of the trigger button (even if we didn't get the release events). */
    if (!(event->buttons() & Qt::LeftButton)) {
        stopResizing();
        return false;
    }

    /* Check for drag distance. */
    if (m_state == PREPAIRING) {
        if ((m_mousePressPos - event->pos()).manhattanLength() < QApplication::startDragDistance()) {
            return false;
        } else {
            startResizing();
        }
    }

    /* Stop resizing if widget was destroyed. */
    if(m_widget.isNull()) {
        stopResizing();
        return false;
    }

    /* Update mouse positions & calculate delta. */
    QPointF currentMouseScenePos = view->mapToScene(event->pos());
    m_lastMouseScenePos = currentMouseScenePos;

    /* Prepare shortcuts. */
    QGraphicsWidget *widget = m_widget.data();
    const QRectF &startGeometry = m_startGeometry;

    /* Calculate new geometry. */
    QLineF delta(widget->mapFromScene(m_mousePressScenePos), widget->mapFromScene(currentMouseScenePos));
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

    /* Ok, we cheat here a bit. When passing constraint to effectiveSizeHint 
     * function, either width or height must not be set. If both are set, then 
     * you get your constraint back and the control never reaches the virtual
     * sizeHint function. 
     * 
     * That is, there is no simple way to implement constant aspect ratio 
     * resizing using Qt-supplied functionality. So we introduce a separate 
     * interface for that. */
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

    event->accept();
    return false; /* Let other instruments receive mouse move events too! */
}

bool ResizingInstrument::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {
    QGraphicsView *view = this->view(viewport);

    if (m_state == INITIAL || !view->isInteractive())
        return false;

    if (event->button() != Qt::LeftButton)
        return false;

    stopResizing();

    event->accept();
    return true;
}

void ResizingInstrument::startResizing() {
    if(m_widget.isNull() || m_view.isNull()) {
        /* Whoops, already destroyed. */
        stopResizing();
        return;
    }

    m_state = RESIZING;

    emit resizingStarted(m_view.data(), m_widget.data());
}

void ResizingInstrument::stopResizing() {
    if(m_state >= PREPAIRING)
        emit resizingProcessFinished(m_view.data(), m_widget.data());
    if(m_state == RESIZING)
        emit resizingFinished(m_view.data(), m_widget.data());
    m_state = INITIAL;
    m_view.clear();
    m_widget.clear();
    m_resizable = NULL;
}
