#include "resizing_instrument.h"

#include <QtWidgets/QGraphicsWidget>
#include <QtCore/QtMath>

#include <ui/common/constrained_resizable.h>
#include <ui/common/frame_section_queryable.h>
#include <ui/common/cursor_cache.h>

#include <ui/graphics/items/standard/graphics_web_view.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <nx/client/core/utils/geometry.h>

using nx::client::core::Geometry;

namespace {

bool itemIsResizableWidget(QGraphicsItem* item)
{
    if (!item->isWidget())
        return false;

    if (!item->acceptedMouseButtons().testFlag(Qt::LeftButton))
        return false;

    const auto widget = static_cast<QGraphicsWidget*>(item);

    auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
    if (mediaWidget && mediaWidget->options().testFlag(
        QnMediaResourceWidget::WindowResizingForbidden))
    {
        return false;
    }

    if (auto webView = qobject_cast<QnGraphicsWebView*>(widget))
        return false;

    return true;
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

void safeUnsetCursor(QGraphicsWidgetPtr widget)
{
    if (widget)
        widget->unsetCursor();
}

WidgetsList subtractWidgets(const WidgetsList& first, const WidgetsList& second)
{
    return first.toSet().subtract(second.toSet()).toList();
}

} // anonymous namespace

uint qHash(const QPointer<QGraphicsWidget>& widget)
{
    return ::qHash(widget.data());
}

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
    m_innerEffectRadius(0.0),
    m_outerEffectRadius(0.0)
{
    dragProcessor()->setStartDragDistance(0);
    dragProcessor()->setStartDragTime(0);
}

ResizingInstrument::~ResizingInstrument()
{
    ensureUninstalled();
}

qreal ResizingInstrument::innerEffectRadius() const
{
    return m_innerEffectRadius;
}

void ResizingInstrument::setInnerEffectRadius(qreal effectRadius)
{
    m_innerEffectRadius = effectRadius;
}

qreal ResizingInstrument::outerEffectRadius() const
{
    return m_outerEffectRadius;
}

void ResizingInstrument::setOuterEffectRadius(qreal effectRadius)
{
    m_outerEffectRadius = effectRadius;
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

    QPoint correctedPos;
    getWidgetAndFrameSection(viewport, event->pos(), section, widget, correctedPos);
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
            Geometry::cwiseDiv(widget->mapFromScene(view->mapToScene(event->pos())), m_startSize);

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

    QPoint correctedPos;
    getWidgetAndFrameSection(viewport, event->pos(), section, widget, correctedPos);

    auto oldTargetWidget = m_affectedWidgets.isEmpty()
        ? QGraphicsWidgetPtr()
        : m_affectedWidgets.last();

    bool needUnsetCursor = !widget || oldTargetWidget != widget || section == Qt::NoSection;
    if (needUnsetCursor)
    {
        for (const auto& w: m_affectedWidgets)
            safeUnsetCursor(w);
        m_affectedWidgets.clear();
    }

    if (!widget || section == Qt::NoSection)
        return false;

    if (widget->cursor().shape() == Qn::calculateHoverCursorShape(section))
        return false;

    /* Note that rotation is estimated using a very simple method here.
     * A better way would be to calculate local rotation at cursor position,
     * but currently there is not need for such precision. */
    const auto rect = widget->rect();
    auto rotation = qRadiansToDegrees(Geometry::atan2(
        widget->mapToScene(rect.topRight()) - widget->mapToScene(rect.topLeft())));
    if (section == Qt::TitleBarArea)
    {
        // We don't want rotated Arrow cursor.
        rotation = 0;
    }
    const auto cursor = QnCursorCache::instance()->cursorForWindowSection(section, rotation, 5.0);

    const auto newAffected = getAffectedWidgets(viewport, correctedPos);
    const auto lostWidgets = subtractWidgets(m_affectedWidgets, newAffected);
    for (const auto& lost: lostWidgets)
        safeUnsetCursor(lost);

    m_affectedWidgets = newAffected;
    NX_ASSERT(m_affectedWidgets.last() == widget);
    for (auto w : m_affectedWidgets)
    {
        NX_ASSERT(w);
        if (w)
            w->setCursor(cursor);
    }

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
        newPos = widget->mapToParent(itemPos - Geometry::cwiseMul(m_startPinPoint, newSize));
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
    QGraphicsWidget*& widget,
    QPoint& correctedPos) const
{
    section = Qt::NoSection;
    widget = nullptr;
    correctedPos = pos;

    if (!dragProcessor()->isWaiting())
        return;

    const auto view = this->view(viewport);
    if (!view->isInteractive())
        return;

    /* Find the item to resize. */
    widget = resizableWidgetAtPos(view, pos);
    if (widget)
    {
        section = queryFrameSection(view, widget, pos, m_innerEffectRadius);
        return;
    }

    QList<QPoint> locationsNearby{
        pos + QPoint(m_outerEffectRadius, m_outerEffectRadius),
        pos + QPoint(-m_outerEffectRadius, m_outerEffectRadius),
        pos + QPoint(-m_outerEffectRadius, -m_outerEffectRadius),
        pos + QPoint(m_outerEffectRadius, -m_outerEffectRadius)
    };

    QGraphicsWidget* widgetNearby = nullptr;
    for (const auto& pos: locationsNearby)
    {
        auto w = resizableWidgetAtPos(view, pos);
        if (!w)
            continue;

        if (!widgetNearby)
        {
            widgetNearby = w;
            correctedPos = pos;
            continue;
        }

        if (w != widgetNearby)
            return;
    }
    if (!widgetNearby)
        return;


    widget = widgetNearby;
    section = queryFrameSection(view, widget, pos, m_outerEffectRadius);
}

QGraphicsWidget* ResizingInstrument::resizableWidgetAtPos(
    QGraphicsView* view,
    const QPoint& pos) const
{
    const auto widget = static_cast<QGraphicsWidget*>(item(view, pos, itemIsResizableWidget));
    if (widget && satisfiesItemConditions(widget))
        return widget;
    return nullptr;
}

Qt::WindowFrameSection ResizingInstrument::queryFrameSection(
    QGraphicsView* view,
    QGraphicsWidget* widget,
    const QPoint& pos,
    qreal effectRadius) const
{
    /* Check frame section. */
    const auto queryable = dynamic_cast<FrameSectionQueryable*>(widget);
    if (!queryable
        && !(widget->windowFlags().testFlag(Qt::Window)
            && widget->windowFlags().testFlag(Qt::WindowTitleHint)))
    {
        return Qt::NoSection; /* Has no decorations and not queryable for frame sections. */
    }

    const auto itemPos = widget->mapFromScene(view->mapToScene(pos));
    if (!queryable)
        return open(widget)->getWindowFrameSectionAt(itemPos);

    const auto radius = mapRectToScene(view, QRectF(0, 0, effectRadius, effectRadius)).width();
    return queryable->windowFrameSectionAt(QRectF(
        itemPos - QPointF(radius, radius), QSizeF(2 * radius, 2 * radius)));
}

WidgetsList ResizingInstrument::getAffectedWidgets(
    QWidget* viewport,
    const QPoint& pos) const
{
    WidgetsList result;

    if (!dragProcessor()->isWaiting())
        return result;

    const auto view = this->view(viewport);
    if (!view->isInteractive())
        return result;

    for (auto item: this->items(view, pos))
    {
        if (!item->isWidget())
            continue;

        result.append(static_cast<QGraphicsWidget*>(item));

        if (itemIsResizableWidget(item))
            break;
    }

    return result;
}
