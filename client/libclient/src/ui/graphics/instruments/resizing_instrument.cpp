#include "resizing_instrument.h"

#include <QtWidgets/QGraphicsWidget>
#include <ui/common/constrained_resizable.h>
#include <ui/common/frame_section_queryable.h>
#include <ui/common/cursor_cache.h>

#include <ui/graphics/items/resource/web_view.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

namespace {

struct ItemIsResizableWidget: public std::unary_function<QGraphicsItem*, bool>
{
    bool operator()(QGraphicsItem* item) const
    {
        if (!item->isWidget() || !item->acceptedMouseButtons().testFlag(Qt::LeftButton))
            return false;

        auto mediaWidget = dynamic_cast<QnMediaResourceWidget*>(item);
        if (mediaWidget && mediaWidget->options().testFlag(
            QnMediaResourceWidget::WindowResizingForbidden))
        {
            return false;
        }

        if (auto webView = dynamic_cast<QnWebView*>(item))
            return false;

        return true;
    }
};

class GraphicsWidget: public QGraphicsWidget
{
public:
    friend class ResizingInstrument;

    Qt::WindowFrameSection getWindowFrameSectionAt(const QPointF& pos) const
    {
        return this->windowFrameSectionAt(pos);
    }
};

GraphicsWidget* open(QGraphicsWidget* widget)
{
    return static_cast<GraphicsWidget*>(widget);
}

} // anonymous namespace

// -------------------------------------------------------------------------- //
// ResizingInfo
// -------------------------------------------------------------------------- //
Qt::WindowFrameSection ResizingInfo::frameSection() const
{
    return m_instrument->m_section;
}

QPoint ResizingInfo::mouseScreenPos() const
{
    return m_info->mouseScreenPos();
}

QPoint ResizingInfo::mouseViewportPos() const
{
    return m_info->mouseViewportPos();
}

QPointF ResizingInfo::mouseScenePos() const
{
    return m_info->mouseScenePos();
}

// -------------------------------------------------------------------------- //
// ResizingInstrument
// -------------------------------------------------------------------------- //
ResizingInstrument::ResizingInstrument(QObject* parent):
    base_type(
        Viewport,
        makeSet(
            QEvent::MouseButtonPress,
            QEvent::MouseMove,
            QEvent::MouseButtonRelease,
            QEvent::Paint),
        parent),
    m_effectRadius(0.0)
{
    dragProcessor()->setStartDragDistance(0);
    dragProcessor()->setStartDragTime(0);
}

ResizingInstrument::~ResizingInstrument()
{
    ensureUninstalled();
}

qreal ResizingInstrument::effectRadius() const
{
    return m_effectRadius;
}

void ResizingInstrument::setEffectRadius(qreal effectRadius)
{
    m_effectRadius = effectRadius;
}

void ResizingInstrument::rehandle()
{
    dragProcessor()->redrag();
}

bool ResizingInstrument::mousePressEvent(QWidget* viewport, QMouseEvent* event)
{
    const auto view = this->view(viewport);

    if (event->button() != Qt::LeftButton)
        return false;

    QGraphicsWidget* widget = nullptr;
    auto section = Qt::NoSection;

    getWidgetAndFrameSection(viewport, event->pos(), section, widget);
    if (section == Qt::NoSection)
        return false;

    /* Ok to go. */
    m_startSize = widget->size();
    m_section = section;
    m_widget = widget;
    m_constrained = dynamic_cast<ConstrainedGeometrically*>(widget);

    if (section == Qt::TitleBarArea)
    {
        m_startPinPoint =
            cwiseDiv(widget->mapFromScene(view->mapToScene(event->pos())), m_startSize);

        /* Make sure we don't get NaNs in startPinPoint. */
        if (qFuzzyIsNull(m_startSize.width()))
            m_startPinPoint.setX(0.0);
        if (qFuzzyIsNull(m_startSize.height()))
            m_startPinPoint.setY(0.0);
    }
    else
    {
        m_startPinPoint = widget->mapToParent(Qn::calculatePinPoint(widget->rect(), section));
    }

    dragProcessor()->mousePressEvent(viewport, event);

    event->accept();
    return false;
}

bool ResizingInstrument::mouseMoveEvent(QWidget* viewport, QMouseEvent* event)
{
    QGraphicsWidget* widget = nullptr;
    auto section = Qt::NoSection;

    getWidgetAndFrameSection(viewport, event->pos(), section, widget);

    if (m_affectedWidget && (m_affectedWidget != widget || section == Qt::NoSection))
    {
        m_affectedWidget->unsetCursor();
        m_affectedWidget.clear();
    }

    if (!widget || section == Qt::NoSection)
        return false;

    const auto cursorShape = Qn::calculateHoverCursorShape(section);
    if (cursorShape == widget->cursor().shape() && cursorShape != Qt::BitmapCursor)
        return false;

    /* Note that rotation is estimated using a very simple method here.
     * A better way would be to calculate local rotation at cursor position,
     * but currently there is not need for such precision. */
    const auto rect = widget->rect();
    const auto rotation = QnGeometry::atan2(
        widget->mapToScene(rect.topRight()) - widget->mapToScene(rect.topLeft())) * 180.0 / M_PI;
    const auto cursor = QnCursorCache::instance()->cursor(cursorShape, rotation, 5.0);

    m_affectedWidget = widget;
    m_affectedWidget->setCursor(cursor);

    event->accept();
    return false;
}

void ResizingInstrument::startDragProcess(DragInfo* info)
{
    ResizingInfo resizingInfo(info, this);
    emit resizingProcessStarted(info->view(), m_widget.data(), &resizingInfo);
}

void ResizingInstrument::startDrag(DragInfo* info)
{
    m_resizingStartedEmitted = false;

    if (m_widget.isNull())
    {
        /* Whoops, already destroyed. */
        dragProcessor()->reset();
        return;
    }

    ResizingInfo resizingInfo(info, this);
    emit resizingStarted(info->view(), m_widget.data(), &resizingInfo);
    m_resizingStartedEmitted = true;
}

void ResizingInstrument::dragMove(DragInfo* info)
{
    /* Stop resizing if widget was destroyed. */
    QGraphicsWidget* widget = m_widget.data();
    if (!widget)
    {
        dragProcessor()->reset();
        return;
    }

    /* Calculate new size. */
    QSizeF newSize;
    if (m_section == Qt::TitleBarArea)
    {
        newSize = widget->size();
    }
    else
    {
        newSize = m_startSize + Qn::calculateSizeDelta(
            widget->mapFromScene(info->mouseScenePos())
                - widget->mapFromScene(info->mousePressScenePos()),
            m_section);
        QSizeF minSize = widget->effectiveSizeHint(Qt::MinimumSize);
        QSizeF maxSize = widget->effectiveSizeHint(Qt::MaximumSize);
        newSize = QSizeF(qBound(minSize.width(), newSize.width(), maxSize.width()),
            qBound(minSize.height(), newSize.height(), maxSize.height()));
        /* We don't handle heightForWidth. */
    }

    /* Calculate new position. */
    QPointF newPos;
    if (m_section == Qt::TitleBarArea)
    {
        QPointF itemPos = widget->mapFromScene(info->mouseScenePos());
        newPos = widget->mapToParent(itemPos - QnGeometry::cwiseMul(m_startPinPoint, newSize));
    }
    else
    {
        newPos = widget->pos() + m_startPinPoint - widget->mapToParent(
            Qn::calculatePinPoint(QRectF(QPointF(0.0, 0.0), newSize), m_section));
    }

    if (m_constrained != NULL)
    {
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
    if (m_section != Qt::TitleBarArea)
    {
        newPos = widget->pos()
            + m_startPinPoint
            - widget->mapToParent(
                Qn::calculatePinPoint(QRectF(QPointF(0.0, 0.0), newSize), m_section));
    }

    /* Change position. */
    widget->setPos(newPos);

    ResizingInfo resizingInfo(info, this);
    emit resizing(info->view(), widget, &resizingInfo);
}

void ResizingInstrument::finishDrag(DragInfo* info)
{
    if (m_resizingStartedEmitted)
    {
        ResizingInfo resizingInfo(info, this);
        emit resizingFinished(info->view(), m_widget.data(), &resizingInfo);
    }

    m_widget.clear();
    m_constrained = NULL;
}

void ResizingInstrument::finishDragProcess(DragInfo* info)
{
    ResizingInfo resizingInfo(info, this);
    emit resizingProcessFinished(info->view(), m_widget.data(), &resizingInfo);
}

void ResizingInstrument::getWidgetAndFrameSection(
    QWidget* viewport,
    const QPoint& pos,
    Qt::WindowFrameSection& section,
    QGraphicsWidget*& widget) const
{
    section = Qt::NoSection;
    widget = nullptr;

    if (!dragProcessor()->isWaiting())
        return;

    const auto view = this->view(viewport);
    if (!view->isInteractive())
        return;

    /* Find the item to resize. */
    widget = static_cast<QGraphicsWidget*>(item(view, pos, ItemIsResizableWidget()));
    if (!widget || !satisfiesItemConditions(widget))
        return;

    /* Check frame section. */
    const auto queryable = dynamic_cast<FrameSectionQueryable*>(widget);
    if (!queryable && !(widget->windowFlags().testFlag(Qt::Window)
            && widget->windowFlags().testFlag(Qt::WindowTitleHint)))
    {
        return; /* Has no decorations and not queryable for frame sections. */
    }

    const auto itemPos = widget->mapFromScene(view->mapToScene(pos));
    if (!queryable)
    {
        section = open(widget)->getWindowFrameSectionAt(itemPos);
        return;
    }

    const auto effectRadius =
        mapRectToScene(view, QRectF(0, 0, m_effectRadius, m_effectRadius)).width();

    section = queryable->windowFrameSectionAt(QRectF(
        itemPos - QPointF(effectRadius, effectRadius),
        QSizeF(2 * effectRadius, 2 * effectRadius)));
}
