#include "zoom_window_instrument.h"

#include <utils/math/color_transformations.h>
#include <utils/common/checked_cast.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/hash.h>
#include <nx/utils/random.h>

#include <nx/client/core/utils/geometry.h>
#include <nx/client/desktop/ui/actions/action.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/animation/opacity_animator.h>
#include <ui/common/constrained_geometrically.h>
#include <ui/common/constrained_resizable.h>
#include <ui/style/globals.h>
#include <ui/style/nx_style.h>
#include <ui/graphics/items/standard/graphics_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/generic/clickable_widgets.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <core/resource_management/resource_runtime_data.h>

#include "instrument_manager.h"
#include "resizing_instrument.h"
#include "selection_item.h"

using namespace nx::client::desktop::ui;
using nx::client::core::Geometry;

namespace {

static constexpr qreal kZoomWindowMinSize = 0.1;
static constexpr qreal kZoomWindowMaxSize = 0.9;
static constexpr qreal kZoomWindowMinAspectRatio = kZoomWindowMinSize / kZoomWindowMaxSize;
static constexpr qreal kZoomWindowMaxAspectRatio = kZoomWindowMaxSize / kZoomWindowMinSize;
static constexpr qreal kMaxZoomWindowAr = 21.0 / 9.0;
static constexpr int kZoomLineWidth = 2;
static constexpr int kFrameWidth = 1;
static constexpr Qt::GlobalColor kFrameColor = Qt::yellow;

bool isZoomAllowed(QGraphicsItem* item)
{
    if (!item->isWidget()) //< Quick check.
        return false;

    auto widget = dynamic_cast<QnMediaResourceWidget*>(item);
    return widget && widget->options().testFlag(QnMediaResourceWidget::ControlZoomWindow);
};

QGraphicsSceneMouseEvent* constructSceneMouseEvent(
    const QMouseEvent* event,
    QGraphicsItem* item,
    QGraphicsView* view,
    QWidget* viewport)
{
    auto type = QEvent::None;

    switch (event->type())
    {
        case QEvent::MouseButtonPress:
            type = QEvent::GraphicsSceneMousePress;
            break;
        case QEvent::MouseButtonRelease:
            type = QEvent::GraphicsSceneMouseRelease;
            break;
        case QEvent::MouseButtonDblClick:
            type = QEvent::GraphicsSceneMouseDoubleClick;
            break;
        case QEvent::MouseMove:
            type = QEvent::GraphicsSceneMouseMove;
            break;
        default:
            return nullptr;
    }

    auto mouseEvent = new QGraphicsSceneMouseEvent(type);
    mouseEvent->setWidget(viewport);
    mouseEvent->setScenePos(view->mapToScene(event->pos()));
    mouseEvent->setPos(item->mapFromScene(mouseEvent->scenePos()));
    mouseEvent->setScreenPos(event->globalPos());
    mouseEvent->setButtons(event->buttons());
    mouseEvent->setButton(event->button());
    mouseEvent->setModifiers(event->modifiers());
    mouseEvent->setSource(event->source());
    mouseEvent->setFlags(event->flags());
    return mouseEvent;
}

enum Direction
{
    NoDirection = 0x0,
    Top         = 0x1,
    Left        = 0x2,
    Bottom      = 0x4,
    Right       = 0x8,
    TopLeft     = Top | Left,
    BottomLeft  = Bottom | Left,
    BottomRight = Bottom | Right,
    TopRight    = Top | Right
};

Direction inversed(Direction value)
{
    switch (value)
    {
        case Direction::Top:
            return Direction::Bottom;
        case Direction::TopLeft:
            return Direction::BottomRight;
        case Direction::Left:
            return Direction::Right;
        case Direction::BottomLeft:
            return Direction::TopRight;
        case Direction::Bottom:
            return Direction::Top;
        case Direction::BottomRight:
            return Direction::TopLeft;
        case Direction::Right:
            return Direction::Left;
        case Direction::TopRight:
            return Direction::BottomLeft;
    }

    return NoDirection;
}

Direction calculateDirection(const QRectF& from, const QRectF& to)
{
    int result = NoDirection;
    if (from.top() >= to.bottom())
        result |= Top;
    else if (from.bottom() <= to.top())
        result |= Bottom;

    if (from.left() >= to.right())
        result |= Left;
    else if (from.right() <= to.left())
        result |= Right;

    return static_cast<Direction>(result);
}

QPointF sourceDirectionPoint(const QRectF& from, Direction direction)
{
    QPointF center = from.center();
    switch (direction)
    {
        case Direction::Top:
            return QPointF(center.x(), from.top());
        case Direction::TopLeft:
            return from.topLeft();
        case Direction::Left:
            return QPointF(from.left(), center.y());
        case Direction::BottomLeft:
            return from.bottomLeft();
        case Direction::Bottom:
            return QPointF(center.x(), from.bottom());
        case Direction::BottomRight:
            return from.bottomRight();
        case Direction::Right:
            return QPointF(from.right(), center.y());
        case Direction::TopRight:
            return from.topRight();
    }

    return center;
}

QPointF targetDirectionPoint(const QRectF& to, Direction direction)
{
    return sourceDirectionPoint(to, inversed(direction));
}

} // namespace

class ZoomOverlayWidget;


// -------------------------------------------------------------------------- //
// ZoomWindowWidget
// -------------------------------------------------------------------------- //
class ZoomWindowWidget: public Instrumented<QnClickableWidget>, public ConstrainedGeometrically
{
    using base_type = Instrumented<QnClickableWidget>;

public:
    ZoomWindowWidget(QGraphicsItem *parent = nullptr, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags),
        m_interactive(true),
        m_frameColor(kFrameColor),
        m_frameWidth(kFrameWidth)
    {
        setWindowFlags(this->windowFlags() | Qt::Window);
        setFlag(ItemIsPanel, false); /* See comment in workbench_display.cpp. */

        setHelpTopic(this, Qn::MainWindow_MediaItem_ZoomWindows_Help);

        updateInteractivity();
    }

    virtual ~ZoomWindowWidget() override;

    QPointer<ZoomOverlayWidget> overlay() const
    {
        return m_overlay;
    }

    void setOverlay(ZoomOverlayWidget* overlay)
    {
        m_overlay = overlay;
    }

    QPointer<QnMediaResourceWidget> zoomWidget() const
    {
        return m_zoomWidget;
    }

    void setZoomWidget(QnMediaResourceWidget* zoomWidget)
    {
        m_zoomWidget = zoomWidget;
    }

    QColor frameColor() const
    {
        return m_frameColor;
    }

    void setFrameColor(const QColor& frameColor)
    {
        m_frameColor = frameColor;
    }

    int frameWidth() const
    {
        return m_frameWidth;
    }

    void setFrameWidth(int frameWidth) //< width is in logical device units
    {
        m_frameWidth = frameWidth;
    }

    bool isInteractive() const
    {
        return m_interactive;
    }

    void setInteractive(bool interactive)
    {
        if (m_interactive == interactive)
            return;

        m_interactive = interactive;

        updateInteractivity();
    }

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent* event) override
    {
        if (!m_interactive)
        {
            base_type::wheelEvent(event);
            return;
        }

        qreal scale = std::pow(2.0, (-event->delta() / 8.0) / 180.0);

        QRectF geometry = this->geometry();
        QPointF fixedPoint = mapToParent(event->pos());
        geometry = constrainedGeometry(
            Geometry::scaled(
                geometry,
                geometry.size() * scale,
                fixedPoint,
                Qt::IgnoreAspectRatio
            ),
            Qt::TitleBarArea,
            fixedPoint
        );

        setGeometry(geometry);

        event->accept();
    }

    virtual QRectF constrainedGeometry(
        const QRectF& geometry,
        Qt::WindowFrameSection pinSection,
        const QPointF& pinPoint) const override;

    virtual void paintWindowFrame(
        QPainter* painter,
        const QStyleOptionGraphicsItem* /*option*/,
        QWidget* /*widget*/) override
    {
        if (m_zoomWidget && m_zoomWidget->options().testFlag(QnResourceWidget::AnalyticsModeSlave))
        {
            const auto originalRegion = qnResourceRuntimeDataManager->layoutItemData(
                m_zoomWidget->item()->uuid(),
                Qn::ItemAnalyticsModeSourceRegionRole).value<QRectF>().
                intersected(QRectF(0, 0, 1, 1));

            // Main frame also must not be painted in this way.
            if (originalRegion.isEmpty())
                return;

            QnScopedPainterOpacityRollback opacityRollback(painter, painter->opacity() * 0.5);
            const QRectF fullRect = Geometry::unsubRect(rect(), m_zoomWidget->item()->zoomRect());
            const QRectF scaled = Geometry::subRect(fullRect, originalRegion);
            QnNxStyle::paintCosmeticFrame(painter, scaled, m_frameColor, 1, 1);

        }

        QnNxStyle::paintCosmeticFrame(painter, rect(), m_frameColor,
            m_frameWidth, m_frameWidth / 2);
    }

    virtual Qn::WindowFrameSections windowFrameSectionsAt(const QRectF& region) const override
    {
        if (!m_interactive)
            return Qn::NoSection;

        Qn::WindowFrameSections result = base_type::windowFrameSectionsAt(region);

        if (result & Qn::SideSections)
            result &= ~Qn::SideSections;
        if (result == Qn::NoSection)
            result = Qn::TitleBarArea;

        return result;
    }

private:
    void updateInteractivity()
    {
        const auto buttons = m_interactive ? Qt::LeftButton : Qt::NoButton;
        setAcceptedMouseButtons(buttons);
        setClickableButtons(buttons);
    }

private:
    bool m_interactive;
    QPointer<ZoomOverlayWidget> m_overlay;
    QPointer<QnMediaResourceWidget> m_zoomWidget;
    QColor m_frameColor;
    int m_frameWidth;
};


// -------------------------------------------------------------------------- //
// ZoomOverlayWidget
// -------------------------------------------------------------------------- //
class ZoomOverlayWidget: public GraphicsWidget
{
    using base_type = GraphicsWidget;

public:
    ZoomOverlayWidget(QGraphicsItem* parent = nullptr, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags),
        m_interactive(true)
    {
        setAcceptedMouseButtons(Qt::NoButton);
    }

    virtual ~ZoomOverlayWidget() override
    {
        auto widgets = m_rectByWidget.keys();
        for (auto widget: widgets)
            removeWidget(widget);
        NX_EXPECT(m_rectByWidget.empty());
    }

    QPointer<QnMediaResourceWidget> target() const
    {
        return m_target;
    }

    void setTarget(QnMediaResourceWidget *target)
    {
        m_target = target;
    }

    void addWidget(ZoomWindowWidget* widget)
    {
        widget->setOverlay(this);
        widget->setParentItem(this);
        widget->setInteractive(m_interactive);
        m_rectByWidget.insert(widget, QRectF(0.0, 0.0, 1.0, 1.0));

        updateLayout(widget);
    }

    void removeWidget(ZoomWindowWidget* widget)
    {
        NX_EXPECT(widget);
        if (!widget)
            return;

        if (widget->parentItem() == this)
        {
            widget->setParentItem(nullptr);
            widget->setOverlay(nullptr);
        }
        m_rectByWidget.remove(widget);
    }

    void setWidgetRect(ZoomWindowWidget* widget, const QRectF& rect, bool updateLayout)
    {
        if (!m_rectByWidget.contains(widget))
            return;

        m_rectByWidget[widget] = rect;

        if (updateLayout)
            this->updateLayout(widget);
    }

    QRectF widgetRect(ZoomWindowWidget* widget) const
    {
        return m_rectByWidget.value(widget, QRectF());
    }

    bool isInteractive() const
    {
        return m_interactive;
    }

    void setInteractive(bool interactive)
    {
        if (m_interactive == interactive)
            return;

        m_interactive = interactive;

        for (auto widget: m_rectByWidget.keys())
            widget->setInteractive(m_interactive);
    }

    virtual void setGeometry(const QRectF& rect) override
    {
        QSizeF oldSize = size();

        base_type::setGeometry(rect);

        if (!qFuzzyEquals(oldSize, size()))
            updateLayout();
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* w) override
    {
        if (m_target && m_target->options().testFlag(QnResourceWidget::InvisibleWidgetOption))
            return;

        if (m_target && m_target->options().testFlag(QnResourceWidget::AnalyticsModeMaster))
        {
            const QRectF masterItemGeometry = m_target->item()->combinedGeometry();

            QnScopedPainterOpacityRollback opacityRollback(painter, painter->opacity() * 0.5);
            QnScopedPainterClipRegionRollback clipRollback(painter, QRegion(rect().toRect()));

            for (auto widget: m_rectByWidget.keys())
            {
                if (m_rectByWidget[widget].isEmpty())
                    continue;

                const QRectF slaveItemGeometry = widget->zoomWidget()->item()->combinedGeometry();
                const Direction direction = calculateDirection(masterItemGeometry, slaveItemGeometry);
                QPointF p1 = sourceDirectionPoint(widget->geometry(), direction);
                QPointF p2 = mapFromScene(
                    targetDirectionPoint(widget->zoomWidget()->geometry(), direction));

                QPen cosmetic(widget->frameColor());
                cosmetic.setCosmetic(true);
                QnScopedPainterPenRollback penRollback(painter, cosmetic);
                painter->drawLine(p1, p2);
            }

        }
    }

private:
    void updateLayout()
    {
        for (auto widget: m_rectByWidget.keys())
            updateLayout(widget);
    }

    void updateLayout(ZoomWindowWidget* widget)
    {
        NX_EXPECT(widget);
        if (widget)
            widget->setGeometry(Geometry::subRect(rect(), m_rectByWidget.value(widget)));
    }

private:
    QHash<ZoomWindowWidget*, QRectF> m_rectByWidget;
    QPointer<QnMediaResourceWidget> m_target;
    bool m_interactive;
};


// -------------------------------------------------------------------------- //
// ZoomWindowWidget
// -------------------------------------------------------------------------- //
ZoomWindowWidget::~ZoomWindowWidget()
{
    if (overlay())
        overlay()->removeWidget(this);
}

QRectF ZoomWindowWidget::constrainedGeometry(
    const QRectF& geometry,
    Qt::WindowFrameSection pinSection,
    const QPointF& pinPoint) const
{
    auto overlayWidget = this->overlay();
    QRectF result = geometry;

    /* Size constraints go first. */
    QSizeF maxSize = geometry.size();
    if (overlayWidget)
    {
        maxSize = Geometry::cwiseMax(
            Geometry::cwiseMin(maxSize, overlayWidget->size() * kZoomWindowMaxSize),
            overlayWidget->size() * kZoomWindowMinSize);
    }

    result = ConstrainedResizable::constrainedGeometry(
        geometry,
        pinSection,
        pinPoint,
        Geometry::expanded(Geometry::aspectRatio(size()), maxSize, Qt::KeepAspectRatio));

    if (!overlayWidget)
        return result;

    // Position constraints go next.
    if (pinSection != Qt::NoSection && pinSection != Qt::TitleBarArea)
    {
        QRectF constraint = overlayWidget->rect();
        const QPointF pinPoint = Qn::calculatePinPoint(geometry, pinSection);
        const auto x = pinPoint.x();
        const auto y = pinPoint.y();

        qreal xScaleFactor = 1.0;
        if (result.left() < constraint.left() && !qFuzzyEquals(result.left(), x))
        {
            xScaleFactor = (constraint.left() - x) / (result.left() - x);
        }
        else if (result.right() > constraint.right() && !qFuzzyEquals(result.right(), x))
        {
            xScaleFactor = (constraint.right() - x) / (result.right() - x);
        }

        qreal yScaleFactor = 1.0;
        if (result.top() < constraint.top() && !qFuzzyEquals(result.top(), y))
        {
            yScaleFactor = (constraint.top() - y) / (result.top() - y);
        }
        else if (result.bottom() > constraint.bottom() && !qFuzzyEquals(result.bottom(), y))
        {
            yScaleFactor = (constraint.bottom() - y) / (result.bottom() - y);
        }

        const qreal scaleFactor = qMin(xScaleFactor, yScaleFactor);

        if (scaleFactor < 0 || qFuzzyIsNull(scaleFactor))
            return Geometry::movedInto(result, overlayWidget->rect());

        return ConstrainedResizable::constrainedGeometry(
            result,
            pinSection,
            pinPoint,
            result.size() * scaleFactor);
    }

    return Geometry::movedInto(result, overlayWidget->rect());
}



// -------------------------------------------------------------------------- //
// ZoomWindowInstrument
// -------------------------------------------------------------------------- //
ZoomWindowInstrument::ZoomWindowInstrument(QObject* parent):
    base_type(
        makeSet(QEvent::MouseButtonPress, QEvent::MouseMove),
        makeSet(),
        makeSet(),
        makeSet(
            QEvent::GraphicsSceneMousePress,
            QEvent::GraphicsSceneMouseRelease,
            QEvent::GraphicsSceneMouseMove),
        parent
    ),
    QnWorkbenchContextAware(parent),
    m_zoomWindowStartedEmitted(false)
{
    dragProcessor()->setStartDragTime(0);

    /* Sensible default. */
    m_colors
        << qnGlobals->warningTextColor()
        << qnGlobals->successTextColor()
        << qnGlobals->errorTextColor();

    connect(display(), &QnWorkbenchDisplay::zoomLinkAdded, this,
        &ZoomWindowInstrument::at_display_zoomLinkAdded);
    connect(display(), &QnWorkbenchDisplay::zoomLinkAboutToBeRemoved, this,
        &ZoomWindowInstrument::at_display_zoomLinkAboutToBeRemoved);
    connect(display(), &QnWorkbenchDisplay::widgetChanged, this,
        &ZoomWindowInstrument::at_display_widgetChanged);
}

ZoomWindowInstrument::~ZoomWindowInstrument()
{
    ensureUninstalled();
}

QVector<QColor> ZoomWindowInstrument::colors() const
{
    return m_colors;
}

void ZoomWindowInstrument::setColors(const QVector<QColor>& colors)
{
    m_colors = colors;
}

QColor ZoomWindowInstrument::nextZoomWindowColor() const
{
    if (m_colors.isEmpty())
        return kFrameColor;

    QSet<QColor> colors;
    for (auto widget: display()->widgets())
        colors.insert(widget->frameDistinctionColor());

    for (const auto& color: m_colors)
    {
        if (!colors.contains(color))
            return color;
    }

    return m_colors[nx::utils::random::number(0, m_colors.size() - 1)];
}

QPointer<ZoomOverlayWidget> ZoomWindowInstrument::overlayWidget(QnMediaResourceWidget* widget) const
{
    return m_dataByWidget[widget].overlayWidget;
}

QPointer<ZoomOverlayWidget> ZoomWindowInstrument::ensureOverlayWidget(QnMediaResourceWidget* widget)
{
    ZoomData& data = m_dataByWidget[widget];

    auto overlayWidget = data.overlayWidget;
    if (overlayWidget)
        return overlayWidget;

    overlayWidget = new ZoomOverlayWidget();
    overlayWidget->setOpacity(1.0);
    overlayWidget->setTarget(widget);
    data.overlayWidget = overlayWidget;

    widget->addOverlayWidget(overlayWidget, QnResourceWidget::UserVisible);
    updateOverlayMode(widget);

    return overlayWidget;
}

QPointer<ZoomWindowWidget> ZoomWindowInstrument::windowWidget(QnMediaResourceWidget *widget) const
{
    return m_dataByWidget[widget].windowWidget;
}

QPointer<QnMediaResourceWidget> ZoomWindowInstrument::target() const
{
    return m_target;
}

QPointer<ZoomWindowWidget> ZoomWindowInstrument::windowTarget() const
{
    return m_windowTarget;
}

QPointer<FixedArSelectionItem> ZoomWindowInstrument::selectionItem() const
{
    return m_selectionItem;
}

void ZoomWindowInstrument::ensureSelectionItem()
{
    if (selectionItem())
        return;

    m_selectionItem = new FixedArSelectionItem();
    selectionItem()->setOpacity(0.0);
    selectionItem()->setBrush(Qt::NoBrush);
    selectionItem()->setElementSize(qnGlobals->workbenchUnitSize() / 64.0);
    selectionItem()->setOptions(FixedArSelectionItem::DrawSideElements);

    const auto currentScene = scene();
    if (currentScene)
        currentScene->addItem(selectionItem());
}

void ZoomWindowInstrument::registerWidget(QnMediaResourceWidget *widget)
{
    connect(widget, &QnResourceWidget::zoomRectChanged, this,
        &ZoomWindowInstrument::at_widget_zoomRectChanged);
    connect(widget, &QnResourceWidget::frameDistinctionColorChanged, this,
        &ZoomWindowInstrument::at_widget_frameColorChanged);
    connect(widget, &QnResourceWidget::aboutToBeDestroyed, this,
        &ZoomWindowInstrument::at_widget_aboutToBeDestroyed);
    connect(widget, &QnResourceWidget::optionsChanged, this,
        &ZoomWindowInstrument::at_widget_optionsChanged);

    /* Initialize frame color if zoom window was loaded from a layout and
     * not created through this instrument. */
    if (widget->isZoomWindow() && !widget->frameDistinctionColor().isValid())
        widget->setFrameDistinctionColor(nextZoomWindowColor());
}

void ZoomWindowInstrument::unregisterWidget(QnMediaResourceWidget* widget)
{
    NX_EXPECT(widget);
    if (!widget)
        return;

    if (!m_dataByWidget.contains(widget))
        return; /* Double unregistration is possible and is OK. */

    widget->disconnect(this);

    m_dataByWidget.remove(widget);

    if (m_target == widget)
    {
        m_target->unsetCursor();
        m_target = nullptr;
    }
}

void ZoomWindowInstrument::registerLink(QnMediaResourceWidget* widget,
    QnMediaResourceWidget* zoomTargetWidget)
{
    ZoomData& data = m_dataByWidget[widget];

    auto overlayWidget = ensureOverlayWidget(zoomTargetWidget);

    ZoomWindowWidget* windowWidget;
    if (m_storedWindowWidget)
    {
        windowWidget = m_storedWindowWidget.data();
        m_storedWindowWidget.clear();
        windowWidget->show();
    }
    else
    {
        windowWidget = new ZoomWindowWidget();
    }
    windowWidget->setZoomWidget(widget);
    windowWidget->setFrameWidth(kZoomLineWidth);
    overlayWidget->addWidget(windowWidget);
    data.windowWidget = windowWidget;
    connect(windowWidget, &ZoomWindowWidget::geometryChanged, this,
        &ZoomWindowInstrument::at_windowWidget_geometryChanged);
    connect(windowWidget, &ZoomWindowWidget::doubleClicked, this,
        &ZoomWindowInstrument::at_windowWidget_doubleClicked);

    updateWindowFromWidget(widget);
}

void ZoomWindowInstrument::unregisterLink(QnMediaResourceWidget* widget, bool deleteWindowWidget)
{
    ZoomData& data = m_dataByWidget[widget];
    if (!data.windowWidget)
        return;

    if (deleteWindowWidget)
    {
        delete data.windowWidget;
    }
    else
    {
        if (auto overlay = data.windowWidget->overlay())
            overlay->removeWidget(data.windowWidget);
        data.windowWidget->setZoomWidget(nullptr);
        data.windowWidget->disconnect(this);
    }
    data.windowWidget = nullptr;
}

void ZoomWindowInstrument::updateOverlayMode(QnMediaResourceWidget* widget)
{
    auto overlayWidget = this->overlayWidget(widget);
    if (!overlayWidget)
        return;

    qreal opacity = 0.0;
    bool interactive = false;
    bool instant = false;
    if (widget == display()->widget(Qn::ZoomedRole))
    {
        /* Leave invisible. */
    }
    else if (widget->options() & QnResourceWidget::DisplayDewarped)
    {
        /* Leave invisible. */
        instant = true;
    }
    else if (widget->options() & (QnResourceWidget::DisplayMotion | QnResourceWidget::DisplayMotionSensitivity))
    {
        /* Leave invisible. */
    }
    else if (widget->options() & QnResourceWidget::DisplayCrosshair)
    {
        /* PTZ mode - transparent, non-interactive. */
        opacity = 0.3;
    }
    else
    {
        opacity = 1.0;
        interactive = true;
    }

    if (instant)
    {
        opacityAnimator(overlayWidget, 1.0)->stop();
        overlayWidget->setOpacity(opacity);
    }
    else
    {
        opacityAnimator(overlayWidget, 1.0)->animateTo(opacity);
    }
    overlayWidget->setInteractive(interactive);
}

void ZoomWindowInstrument::updateWindowFromWidget(QnMediaResourceWidget* widget)
{
    if (m_processingWidgets.contains(widget))
        return;
    m_processingWidgets.insert(widget);

    auto windowWidget = this->windowWidget(widget);
    auto overlayWidget = windowWidget ? windowWidget->overlay() : nullptr;

    if (windowWidget && overlayWidget)
    {
        overlayWidget->setWidgetRect(windowWidget, widget->zoomRect(), true);
        windowWidget->setFrameColor(widget->frameDistinctionColor().lighter(110));
    }

    m_processingWidgets.remove(widget);
}

void ZoomWindowInstrument::updateWidgetFromWindow(ZoomWindowWidget* windowWidget)
{
    auto overlayWidget = windowWidget->overlay();
    auto zoomWidget = windowWidget->zoomWidget();

    if (m_processingWidgets.contains(zoomWidget))
        return;
    m_processingWidgets.insert(zoomWidget);

    if (overlayWidget && zoomWidget && !qFuzzyIsNull(overlayWidget->size()))
    {
        QRectF zoomRect = Geometry::cwiseDiv(windowWidget->geometry(), overlayWidget->size());
        overlayWidget->setWidgetRect(windowWidget, zoomRect, false);
        emit zoomRectChanged(zoomWidget, zoomRect);
    }

    m_processingWidgets.remove(zoomWidget);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void ZoomWindowInstrument::installedNotify()
{
    NX_ASSERT(!selectionItem());

    m_resizingInstrument = manager()->instrument<ResizingInstrument>();
    if (m_resizingInstrument)
    {
        connect(m_resizingInstrument, &ResizingInstrument::resizingStarted, this,
            &ZoomWindowInstrument::at_resizingStarted);
        connect(m_resizingInstrument, &ResizingInstrument::resizing, this,
            &ZoomWindowInstrument::at_resizing);
        connect(m_resizingInstrument, &ResizingInstrument::resizingFinished, this,
            &ZoomWindowInstrument::at_resizingFinished);
    }

    base_type::installedNotify();
}

void ZoomWindowInstrument::aboutToBeUninstalledNotify()
{
    base_type::aboutToBeUninstalledNotify();

    if (auto resizingInstrument = manager()->instrument<ResizingInstrument>())
        resizingInstrument->disconnect(this);

    if (selectionItem())
        delete selectionItem();
}

bool ZoomWindowInstrument::registeredNotify(QGraphicsItem* item)
{
    if (!base_type::registeredNotify(item))
        return false;

    if (auto widget = dynamic_cast<QnMediaResourceWidget*>(item))
    {
        registerWidget(widget);
        return true;
    }

    return false;
}

void ZoomWindowInstrument::unregisteredNotify(QGraphicsItem* item)
{
    base_type::unregisteredNotify(item);

    /* Note that this dynamic_cast may fail because media resource widget is
     * already being destroyed. We don't care as in this case it will already
     * be unregistered in aboutToBeDestroyed handler. */
    if (auto widget = dynamic_cast<QnMediaResourceWidget*>(item))
        unregisterWidget(widget);
}

bool ZoomWindowInstrument::mousePressEvent(QWidget* viewport, QMouseEvent* /*event*/)
{
    m_viewport = viewport;
    return false;
}

bool ZoomWindowInstrument::mousePressEvent(QGraphicsItem* item, QGraphicsSceneMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return false;

    const auto target = checked_cast<QnMediaResourceWidget*>(item);

    if (m_target && m_target != target)
    {
        m_target->unsetCursor();
        m_target = nullptr;
    }

    if (!target || !target->options().testFlag(QnMediaResourceWidget::ControlZoomWindow))
        return false;

    m_target = target;
    m_target->setCursor(Qt::CrossCursor);

    return base_type::mousePressEvent(item, event);
}

bool ZoomWindowInstrument::mouseMoveEvent(QWidget* viewport, QMouseEvent* event)
{
    auto view = this->view(viewport);
    auto target = static_cast<QnMediaResourceWidget*>(
        this->item(view, event->pos(), isZoomAllowed));

    if (!event->buttons().testFlag(Qt::LeftButton))
    {
        if (m_target && m_target != target)
        {
            m_target->unsetCursor();
            m_target = nullptr;
        }
    }

    if (!target)
        return false;

    if (!m_target)
        m_target = target;

    m_target->setCursor(Qt::CrossCursor);

    event->accept();
    return false;
}

void ZoomWindowInstrument::startDragProcess(DragInfo* /*info*/)
{
    emit zoomWindowProcessStarted(target());
}

void ZoomWindowInstrument::startDrag(DragInfo* /*info*/)
{
    m_zoomWindowStartedEmitted = false;

    if (!target())
    {
        /* Whoops, already destroyed. */
        reset();
        return;
    }

    ensureSelectionItem();
    selectionItem()->setParentItem(target());
    selectionItem()->setViewport(m_viewport.data());
    opacityAnimator(selectionItem())->stop();
    selectionItem()->setOpacity(1.0);

    m_zoomWindowColor = nextZoomWindowColor();

    QPen pen = selectionItem()->pen();
    pen.setColor(m_zoomWindowColor); // TODO: #Elric write in 1 line?
    selectionItem()->setPen(pen);

    // Everything else will be initialized in the first call to drag().

    emit zoomWindowStarted(target());
    m_zoomWindowStartedEmitted = true;
}

void ZoomWindowInstrument::dragMove(DragInfo* info)
{
    if (!target())
    {
        reset();
        return;
    }

    qreal originalAr = Geometry::aspectRatio(target()->size())
        / Geometry::aspectRatio(target()->channelLayout()->size());

    // Here are the special algorithm by #rvasilenko
    int resizeCoef = 1;
    qreal targetAr = originalAr;
    while (targetAr > kMaxZoomWindowAr)
    {
        ++resizeCoef;
        targetAr = originalAr / resizeCoef;
    }

    ensureSelectionItem();
    selectionItem()->setGeometry(
        info->mousePressItemPos(),
        info->mouseItemPos(),
        targetAr,
        target()->rect());
}

void ZoomWindowInstrument::finishDrag(DragInfo* /*info*/)
{
    if (target())
    {
        ensureSelectionItem();
        opacityAnimator(selectionItem(), 4.0)->animateTo(0.0);

        QRectF zoomRect = Geometry::cwiseDiv(selectionItem()->rect(), target()->size());
        if (qFuzzyIsNull(zoomRect.width()))
            zoomRect.setWidth(kZoomWindowMinSize);
        if (qFuzzyIsNull(zoomRect.height()))
            zoomRect.setHeight(kZoomWindowMinSize);

        qreal ar = Geometry::aspectRatio(zoomRect);
        ar = qBound(kZoomWindowMinAspectRatio, ar, kZoomWindowMaxAspectRatio);

        if (zoomRect.width() < kZoomWindowMinSize || zoomRect.height() < kZoomWindowMinSize)
        {
            const QSizeF minSize(kZoomWindowMinSize, kZoomWindowMinSize);
            zoomRect = Geometry::expanded(ar, minSize, zoomRect.center(), Qt::KeepAspectRatioByExpanding);
        }
        else if (zoomRect.width() > kZoomWindowMaxSize || zoomRect.height() > kZoomWindowMaxSize)
        {
            const QSizeF maxSize(kZoomWindowMaxSize, kZoomWindowMaxSize);
            zoomRect = Geometry::expanded(ar, maxSize, zoomRect.center(), Qt::KeepAspectRatio);
        }

        // Coordinates are relative to source rect
        zoomRect = Geometry::movedInto(zoomRect, QRectF(0.0, 0.0, 1.0, 1.0));

        emit zoomRectCreated(target(), m_zoomWindowColor, zoomRect);
    }

    if (m_zoomWindowStartedEmitted)
        emit zoomWindowFinished(target());
}

void ZoomWindowInstrument::finishDragProcess(DragInfo* /*info*/)
{
    emit zoomWindowProcessFinished(target());
}

void ZoomWindowInstrument::at_widget_aboutToBeDestroyed()
{
    unregisterWidget(checked_cast<QnMediaResourceWidget*>(sender()));
}

void ZoomWindowInstrument::at_widget_zoomRectChanged()
{
    updateWindowFromWidget(checked_cast<QnMediaResourceWidget*>(sender()));
}

void ZoomWindowInstrument::at_widget_frameColorChanged()
{
    updateWindowFromWidget(checked_cast<QnMediaResourceWidget*>(sender()));
}

void ZoomWindowInstrument::at_widget_optionsChanged()
{
    updateOverlayMode(checked_cast<QnMediaResourceWidget*>(sender()));
}

void ZoomWindowInstrument::at_windowWidget_geometryChanged()
{
    updateWidgetFromWindow(checked_cast<ZoomWindowWidget*>(sender()));
}

void ZoomWindowInstrument::at_windowWidget_doubleClicked()
{
    auto windowWidget = checked_cast<ZoomWindowWidget*>(sender());

    // TODO: #Elric does not belong here.
    if (auto zoomWidget = windowWidget->zoomWidget())
        workbench()->setItem(Qn::ZoomedRole, zoomWidget->item());
}

void ZoomWindowInstrument::at_display_widgetChanged(Qn::ItemRole role)
{
    if (role != Qn::ZoomedRole)
        return;

    if (m_zoomedWidget)
        updateOverlayMode(m_zoomedWidget.data());

    m_zoomedWidget = qobject_cast<QnMediaResourceWidget*>(display()->widget(role));

    if (m_zoomedWidget)
        updateOverlayMode(m_zoomedWidget.data());
}

void ZoomWindowInstrument::at_display_zoomLinkAdded(
    QnResourceWidget* widget,
    QnResourceWidget* zoomTargetWidget)
{
    auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget);
    auto mediaZoomTargetWidget = qobject_cast<QnMediaResourceWidget*>(zoomTargetWidget);

    if (mediaWidget && mediaZoomTargetWidget)
        registerLink(mediaWidget, mediaZoomTargetWidget);
}

void ZoomWindowInstrument::at_display_zoomLinkAboutToBeRemoved(
    QnResourceWidget* widget,
    QnResourceWidget* /*zoomTargetWidget*/)
{
    if (auto mediaWidget = qobject_cast<QnMediaResourceWidget*>(widget))
        unregisterLink(mediaWidget);
}

void ZoomWindowInstrument::at_resizingStarted(
    QGraphicsView* /*view*/,
    QGraphicsWidget* widget,
    ResizingInfo* info)
{
    if (info->frameSection() != Qt::TitleBarArea)
        return;

    m_windowTarget = checked_cast<ZoomWindowWidget*>(widget);
}

void ZoomWindowInstrument::at_resizing(
    QGraphicsView* view,
    QGraphicsWidget* /*widget*/,
    ResizingInfo* info)
{
    if (!windowTarget() || windowTarget() == m_storedWindowWidget.data())
        return;

    auto newTargetWidget = this->item<QnMediaResourceWidget>(view, info->mouseViewportPos());
    if (!newTargetWidget || newTargetWidget == windowTarget()->overlay()->target())
        return;

    auto action = menu()->action(action::CreateZoomWindowAction);
    if (!action || action->checkCondition(action->scope(), newTargetWidget) != action::EnabledAction)
        return;

    auto widget = windowTarget()->zoomWidget();
    auto oldTargetWidget = windowTarget()->overlay()->target();

    /* Preserve zoom window widget. */
    windowTarget()->hide();
    unregisterLink(widget, false);

    if (m_storedWindowWidget)
        delete m_storedWindowWidget.data(); /* Just to feel safe that we don't leak memory. */
    m_storedWindowWidget = windowTarget();

    QRectF zoomRect = widget->zoomRect();
    NX_EXPECT(oldTargetWidget);
    QSizeF oldLayoutSize = oldTargetWidget->channelLayout()->size();
    QSizeF newLayoutSize = newTargetWidget->channelLayout()->size();
    if (oldLayoutSize != newLayoutSize)
    {
        QSizeF zoomSize = Geometry::cwiseDiv(
            Geometry::cwiseMul(zoomRect.size(), oldLayoutSize), newLayoutSize);
        zoomRect = Geometry::movedInto(
            QRectF(zoomRect.topLeft(), zoomSize), QRectF(0.0, 0.0, 1.0, 1.0));
    }

    emit zoomTargetChanged(widget, zoomRect, newTargetWidget);
    m_resizingInstrument->rehandle();
}

void ZoomWindowInstrument::at_resizingFinished(
    QGraphicsView* /*view*/,
    QGraphicsWidget* /*widget*/,
    ResizingInfo* /*info*/)
{
    m_windowTarget.clear();
}


