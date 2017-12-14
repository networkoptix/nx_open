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
#include <core/resource/media_resource.h>

#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include "selection_item.h"

namespace {

bool motionSelectionEnabled(QnMediaResourceWidget* widget)
{
    NX_ASSERT(widget);
    if (!widget)
        return false;

    const auto options = widget->options();
    return options.testFlag(QnMediaResourceWidget::DisplayMotion)
        || options.testFlag(QnMediaResourceWidget::DisplayMotionSensitivity);
}

// This way we detect widget that can possibly have DisplayMotion enabled
bool widgetWithMotion(QGraphicsItem* item)
{
    if (const auto widget = dynamic_cast<QnMediaResourceWidget*>(item))
    {
        return widget->resource()->toResource()->hasFlags(Qn::motion)
            && !widget->isZoomWindow();
    }
    return false;
}

// Some widgets (e.g. timeline) must block motion selection
bool blocksMotionSelection(QGraphicsItem* item)
{
    if (const auto object = dynamic_cast<QGraphicsObject*>(item))
        return object->property(Qn::BlockMotionSelection).isValid();
    return false;
}

// Auto-start drag by timer must be disabled.
static constexpr int kStartDragTimeMs = -1;

// Drag must be started as early as possible when we can distinguish move from click.
static constexpr int kStartDragDistance = 5;

} // namespace


MotionSelectionInstrument::MotionSelectionInstrument(QObject *parent):
    base_type(Viewport,
        makeSet(QEvent::MouseButtonPress,
            QEvent::MouseMove,
            QEvent::MouseButtonRelease,
            QEvent::Paint),
        parent),
    m_selectionModifiers(Qt::NoModifier),
    m_multiSelectionModifiers(Qt::ControlModifier)
{
    /* Default-initialize pen & brush from temporary selection item. */
    QScopedPointer<SelectionItem> item(new SelectionItem());
    m_pen = item->pen();
    m_brush = item->brush();

    dragProcessor()->setStartDragTime(kStartDragTimeMs);
    dragProcessor()->setStartDragDistance(kStartDragDistance);
}

MotionSelectionInstrument::~MotionSelectionInstrument()
{
    ensureUninstalled();
}

void MotionSelectionInstrument::setPen(const QPen &pen)
{
    m_pen = pen;

    if (selectionItem())
        selectionItem()->setPen(pen);
}

QPen MotionSelectionInstrument::pen() const
{
    return m_pen;
}

void MotionSelectionInstrument::setBrush(const QBrush &brush)
{
    m_brush = brush;

    if (selectionItem())
        selectionItem()->setBrush(brush);
}

QBrush MotionSelectionInstrument::brush() const
{
    return m_brush;
}

void MotionSelectionInstrument::setSelectionModifiers(Qt::KeyboardModifiers modifiers)
{
    m_selectionModifiers = modifiers;
}

Qt::KeyboardModifiers MotionSelectionInstrument::selectionModifiers() const
{
    return m_selectionModifiers;
}

void MotionSelectionInstrument::setMultiSelectionModifiers(Qt::KeyboardModifiers modifiers)
{
    m_multiSelectionModifiers = modifiers;
}

Qt::KeyboardModifiers MotionSelectionInstrument::multiSelectionModifiers() const
{
    return m_multiSelectionModifiers;
}

void MotionSelectionInstrument::installedNotify()
{
    NX_ASSERT(selectionItem() == nullptr);
    ensureSelectionItem();
    base_type::installedNotify();
}

void MotionSelectionInstrument::aboutToBeDisabledNotify()
{
    setWidget(nullptr);
    base_type::aboutToBeDisabledNotify();
}

void MotionSelectionInstrument::aboutToBeUninstalledNotify()
{
    base_type::aboutToBeUninstalledNotify();
    delete selectionItem();
}

void MotionSelectionInstrument::ensureSelectionItem()
{
    if (selectionItem())
        return;

    m_selectionItem = new SelectionItem();
    selectionItem()->setVisible(false);
    selectionItem()->setPen(m_pen);
    selectionItem()->setBrush(m_brush);

    if (scene())
        scene()->addItem(selectionItem());
}

void MotionSelectionInstrument::updateWidgetUnderCursor(QWidget* viewport, QMouseEvent* event)
{
    const auto view = this->view(viewport);

    const auto blocker = this->item(view, event->pos(), blocksMotionSelection);
    if (blocker)
    {
        setWidget(nullptr);
        return;
    }

    const auto widget = dynamic_cast<QnMediaResourceWidget*>(
        this->item(view, event->pos(), widgetWithMotion));
    setWidget(widget);
}

void MotionSelectionInstrument::updateCursor()
{
    if (!m_itemUnderMouse)
        return;

    if (m_widget && motionSelectionEnabled(m_widget) &&
        (dragProcessor()->isRunning() || !m_buttonUnderMouse))
    {
        m_itemUnderMouse->setCursor(Qt::CrossCursor);
        return;
    }

    m_itemUnderMouse->unsetCursor();
}

void MotionSelectionInstrument::setWidget(QnMediaResourceWidget* widget)
{
    if (m_widget == widget)
        return;

    if (m_widget)
        m_widget->disconnect(this);

    m_widget = widget;

    if (m_widget)
    {
        connect(m_widget, &QnResourceWidget::optionsChanged, this,
            &MotionSelectionInstrument::updateCursor);
        connect(m_widget, &QObject::destroyed, this,
            &MotionSelectionInstrument::updateCursor);
    }

    updateCursor();
}

void MotionSelectionInstrument::setItemUnderMouse(QGraphicsWidget* item)
{
    if (m_itemUnderMouse == item)
        return;

    if (m_itemUnderMouse)
        m_itemUnderMouse->unsetCursor();

    m_itemUnderMouse = item;

    updateCursor();
}

Qt::KeyboardModifiers MotionSelectionInstrument::selectionModifiers(
    QnMediaResourceWidget* target) const
{
    if (!target)
        return m_selectionModifiers;

    return static_cast<Qt::KeyboardModifiers>(qvariant_cast<int>(
        target->property(Qn::MotionSelectionModifiers), m_selectionModifiers));
}

bool MotionSelectionInstrument::mousePressEvent(QWidget *viewport, QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton)
        return false;

    // Handle situation when we stopped drag over another widget
    updateWidgetUnderCursor(viewport, event);

    if (!m_widget)
        return false;

    const auto selectionModifiers = this->selectionModifiers(m_widget);
    if ((event->modifiers() & selectionModifiers) != selectionModifiers)
        return false;

    dragProcessor()->mousePressEvent(viewport, event);

    event->accept();
    return false;
}

void MotionSelectionInstrument::updateButtonUnderCursor(QWidget* viewport, QMouseEvent* event)
{
    const auto graphicsView = view(viewport);
    const auto buttonItem = item(graphicsView, event->pos(),
        [](QGraphicsItem* item)
        {
            return dynamic_cast<QnImageButtonWidget*>(item);
        });

    const auto button = dynamic_cast<QnImageButtonWidget*>(buttonItem);
    if (button == m_buttonUnderMouse)
        return;

    m_buttonUnderMouse = button;

    updateCursor();
}

bool MotionSelectionInstrument::mouseMoveEvent(QWidget* viewport, QMouseEvent* event)
{
    auto view = this->view(viewport);

    // Really UiElementsWidget always getting here for main scene, resource widget for motion tab.
    auto item = dynamic_cast<QGraphicsWidget*>(
        this->item(view, event->pos(), [](QGraphicsItem* item) { return item->isWidget(); }));
    setItemUnderMouse(item);

    updateButtonUnderCursor(viewport, event);

    if (m_widget)
        dragProcessor()->mouseMoveEvent(viewport, event);

    // Make sure selection will not stop while we are dragging over widget.
    const bool isDrag = dragProcessor()->isRunning() && event->buttons().testFlag(Qt::LeftButton);
    if (!isDrag)
        updateWidgetUnderCursor(viewport, event);

    event->accept();
    return false;
}

bool MotionSelectionInstrument::mouseReleaseEvent(QWidget* viewport, QMouseEvent* event)
{
    const auto result = base_type::mouseReleaseEvent(viewport, event);
    updateWidgetUnderCursor(viewport, event);
    updateCursor();
    return result;
}

bool MotionSelectionInstrument::paintEvent(QWidget* viewport, QPaintEvent* event)
{
    if (!target())
    {
        dragProcessor()->reset();
        return false;
    }

    return base_type::paintEvent(viewport, event);
}

void MotionSelectionInstrument::startDragProcess(DragInfo* info)
{
    emit selectionProcessStarted(info->view(), target());
}

void MotionSelectionInstrument::startDrag(DragInfo* info)
{
    m_selectionStartedEmitted = false;
    m_gridRect = QRect();

    if (!target())
    {
        /* Whoops, already destroyed. */
        dragProcessor()->reset();
        return;
    }

    if ((info->modifiers() & m_multiSelectionModifiers) != m_multiSelectionModifiers)
    {
        emit motionRegionCleared(info->view(), target());
    }

    ensureSelectionItem();
    selectionItem()->setParentItem(target());
    selectionItem()->setViewport(info->view()->viewport());
    selectionItem()->setVisible(true);
    /* Everything else will be initialized in the first call to drag(). */

    emit selectionStarted(info->view(), target());
    m_selectionStartedEmitted = true;
}

void MotionSelectionInstrument::dragMove(DragInfo* info)
{
    if (!target())
    {
        dragProcessor()->reset();
        return;
    }
    ensureSelectionItem();

    QPointF mouseOrigin = target()->mapFromScene(info->mousePressScenePos());
    QPointF mouseCorner = target()->mapFromScene(info->mouseScenePos());

    QPointF gridStep = target()->mapFromMotionGrid(QPoint(1, 1)) - target()->mapFromMotionGrid(QPoint(0, 0));
    QPointF mouseDelta = mouseOrigin - mouseCorner;
    if (qAbs(mouseDelta.x()) < qAbs(gridStep.x()) / 2 && qAbs(mouseDelta.y()) < qAbs(gridStep.y()) / 2)
    {
        selectionItem()->setRect(QPointF(0, 0), QPointF(0, 0)); /* Ignore small deltas. */
        m_gridRect = QRect();
    }
    else
    {
        QPoint gridOrigin = target()->mapToMotionGrid(mouseOrigin);
        QPoint gridCorner = target()->mapToMotionGrid(mouseCorner) + QPoint(1, 1);

        if (gridCorner.x() <= gridOrigin.x())
        {
            gridCorner -= QPoint(1, 0);
            gridOrigin += QPoint(1, 0);
        }

        if (gridCorner.y() <= gridOrigin.y())
        {
            gridCorner -= QPoint(0, 1);
            gridOrigin += QPoint(0, 1);
        }

        QPointF origin = target()->mapFromMotionGrid(gridOrigin);
        QPointF corner = target()->mapFromMotionGrid(gridCorner);
        selectionItem()->setRect(origin, corner);

        m_gridRect = QRect(gridOrigin, gridCorner).normalized().adjusted(0, 0, -1, -1);
    }
}

void MotionSelectionInstrument::finishDrag(DragInfo* info)
{
    ensureSelectionItem();
    if (target())
    {
        emit motionRegionSelected(
            info->view(),
            target(),
            m_gridRect
        );
    }

    selectionItem()->setVisible(false);
    selectionItem()->setParentItem(NULL);

    if (m_selectionStartedEmitted)
        emit selectionFinished(info->view(), target());
}

void MotionSelectionInstrument::finishDragProcess(DragInfo* info)
{
    emit selectionProcessFinished(info->view(), target());
}

SelectionItem* MotionSelectionInstrument::selectionItem() const
{
    return m_selectionItem.data();
}

QnMediaResourceWidget* MotionSelectionInstrument::target() const
{
    return m_widget.data();
}
