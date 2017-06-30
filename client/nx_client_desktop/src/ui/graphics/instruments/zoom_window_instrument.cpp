#include "zoom_window_instrument.h"

#include <cassert>

#include <utils/math/color_transformations.h>
#include <utils/common/checked_cast.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/hash.h>
#include <nx/utils/random.h>

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

#include "instrument_manager.h"
#include "resizing_instrument.h"
#include "selection_item.h"

using namespace nx::client::desktop::ui;

namespace {

    constexpr qreal kZoomWindowMinSize = 0.1;
    constexpr qreal kZoomWindowMaxSize = 0.9;
    constexpr qreal kZoomWindowMinAspectRatio = kZoomWindowMinSize / kZoomWindowMaxSize;
    constexpr qreal kZoomWindowMaxAspectRatio = kZoomWindowMaxSize / kZoomWindowMinSize;
    static const qreal kMaxZoomWindowAr = 21.0 / 9.0;
    const int kZoomLineWidth = 2;

    const auto isZoomAllowed = [](QGraphicsItem* item)
        {
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

} // namespace

class ZoomOverlayWidget;


// -------------------------------------------------------------------------- //
// ZoomWindowWidget
// -------------------------------------------------------------------------- //
class ZoomWindowWidget: public Instrumented<QnClickableWidget>, public ConstrainedGeometrically {
    typedef Instrumented<QnClickableWidget> base_type;
public:
    ZoomWindowWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags),
        m_interactive(true),
        m_frameColor(Qt::yellow),
        m_frameWidth(1)
    {
        setWindowFlags(this->windowFlags() | Qt::Window);
        setFlag(ItemIsPanel, false); /* See comment in workbench_display.cpp. */

        setHelpTopic(this, Qn::MainWindow_MediaItem_ZoomWindows_Help);

        updateInteractivity();
    }

    virtual ~ZoomWindowWidget();

    ZoomOverlayWidget *overlay() const {
        return m_overlay.data();
    }

    void setOverlay(ZoomOverlayWidget *overlay) {
        m_overlay = overlay;
    }

    QnMediaResourceWidget *zoomWidget() const {
        return m_zoomWidget.data();
    }

    void setZoomWidget(QnMediaResourceWidget *zoomWidget) {
        m_zoomWidget = zoomWidget;
    }

    QColor frameColor() const
    {
        return m_frameColor;
    }

    void setFrameColor(const QColor &frameColor)
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

    bool isInteractive() const {
        return m_interactive;
    }

    void setInteractive(bool interactive) {
        if(m_interactive == interactive)
            return;

        m_interactive = interactive;

        updateInteractivity();
    }

protected:
    virtual void wheelEvent(QGraphicsSceneWheelEvent *event) override {
        if(!m_interactive) {
            base_type::wheelEvent(event);
            return;
        }

        qreal scale = std::pow(2.0, (-event->delta() / 8.0) / 180.0);

        QRectF geometry = this->geometry();
        QPointF fixedPoint = mapToParent(event->pos());
        geometry = constrainedGeometry(
            QnGeometry::scaled(
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

    virtual QRectF constrainedGeometry(const QRectF &geometry, Qt::WindowFrameSection pinSection, const QPointF &pinPoint = QPointF()) const override;

    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        QnNxStyle::paintCosmeticFrame(painter, rect(), m_frameColor,
            m_frameWidth, m_frameWidth / 2);
    }

    virtual Qn::WindowFrameSections windowFrameSectionsAt(const QRectF &region) const override {
        if(!m_interactive)
            return Qn::NoSection;

        Qn::WindowFrameSections result = base_type::windowFrameSectionsAt(region);

        if(result & Qn::SideSections)
            result &= ~Qn::SideSections;
        if(result == Qn::NoSection)
            result = Qn::TitleBarArea;

        return result;
    }

private:
    void updateInteractivity() {
        if(m_interactive) {
            setAcceptedMouseButtons(Qt::LeftButton);
            setClickableButtons(Qt::LeftButton);
        } else {
            setAcceptedMouseButtons(Qt::NoButton);
            setClickableButtons(Qt::NoButton);
        }
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
class ZoomOverlayWidget: public GraphicsWidget {
    typedef GraphicsWidget base_type;
public:
    ZoomOverlayWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0):
        base_type(parent, windowFlags),
        m_interactive(true)
    {
        setAcceptedMouseButtons(0);
    }

    QnMediaResourceWidget *target() const {
        return m_target.data();
    }

    void setTarget(QnMediaResourceWidget *target) {
        m_target = target;
    }

    void addWidget(ZoomWindowWidget *widget) {
        widget->setOverlay(this);
        widget->setParentItem(this);
        widget->setInteractive(m_interactive);
        m_rectByWidget.insert(widget, QRectF(0.0, 0.0, 1.0, 1.0));

        updateLayout(widget);
    }

    void removeWidget(ZoomWindowWidget *widget) {
        if(widget->parentItem() == this) {
            widget->setParentItem(NULL);
            widget->setOverlay(NULL);
        }
        m_rectByWidget.remove(widget);
    }

    void setWidgetRect(ZoomWindowWidget *widget, const QRectF &rect, bool updateLayout) {
        if(!m_rectByWidget.contains(widget))
            return;

        m_rectByWidget[widget] = rect;

        if(updateLayout)
            this->updateLayout(widget);
    }

    QRectF widgetRect(ZoomWindowWidget *widget) const {
        return m_rectByWidget.value(widget, QRectF());
    }

    bool isInteractive() const {
        return m_interactive;
    }

    void setInteractive(bool interactive) {
        if(m_interactive == interactive)
            return;

        m_interactive = interactive;

        foreach(ZoomWindowWidget *widget, m_rectByWidget.keys())
            widget->setInteractive(m_interactive);
    }

    virtual void setGeometry(const QRectF &rect) override {
        QSizeF oldSize = size();

        base_type::setGeometry(rect);

        if(!qFuzzyEquals(oldSize, size()))
            updateLayout();
    }

private:
    void updateLayout() {
        foreach(ZoomWindowWidget *widget, m_rectByWidget.keys())
            updateLayout(widget);
    }

    void updateLayout(ZoomWindowWidget *widget) {
        widget->setGeometry(QnGeometry::subRect(rect(), m_rectByWidget.value(widget)));
    }

private:
    QHash<ZoomWindowWidget *, QRectF> m_rectByWidget;
    QPointer<QnMediaResourceWidget> m_target;
    bool m_interactive;
};


// -------------------------------------------------------------------------- //
// ZoomWindowWidget
// -------------------------------------------------------------------------- //
ZoomWindowWidget::~ZoomWindowWidget() {
    if(overlay())
        overlay()->removeWidget(this);
}

QRectF ZoomWindowWidget::constrainedGeometry(const QRectF &geometry, Qt::WindowFrameSection pinSection, const QPointF &pinPoint) const {
    ZoomOverlayWidget *overlayWidget = this->overlay();
    QRectF result = geometry;

    /* Size constraints go first. */
    QSizeF maxSize = geometry.size();
    if(overlayWidget)
        maxSize = QnGeometry::cwiseMax(QnGeometry::cwiseMin(maxSize, overlayWidget->size() * kZoomWindowMaxSize), overlayWidget->size() * kZoomWindowMinSize);

    result = ConstrainedResizable::constrainedGeometry(geometry, pinSection, pinPoint, QnGeometry::expanded(QnGeometry::aspectRatio(size()), maxSize, Qt::KeepAspectRatio));

    /* Position constraints go next. */
    if (overlayWidget) {
        if (pinSection != Qt::NoSection && pinSection != Qt::TitleBarArea) {
            QRectF constraint = overlayWidget->rect();
            QPointF pinPoint = Qn::calculatePinPoint(geometry, pinSection);

            qreal xScaleFactor = 1.0;
            if(result.left() < constraint.left() && !qFuzzyEquals(result.left(), pinPoint.x())) {
                xScaleFactor = (constraint.left() - pinPoint.x()) / (result.left() - pinPoint.x());
            } else if(result.right() > constraint.right() && !qFuzzyEquals(result.right(), pinPoint.x())) {
                xScaleFactor = (constraint.right() - pinPoint.x()) / (result.right() - pinPoint.x());
            }

            qreal yScaleFactor = 1.0;
            if(result.top() < constraint.top() && !qFuzzyEquals(result.top(), pinPoint.y())) {
                yScaleFactor = (constraint.top() - pinPoint.y()) / (result.top() - pinPoint.y());
            } else if(result.bottom() > constraint.bottom() && !qFuzzyEquals(result.bottom(), pinPoint.y())) {
                yScaleFactor = (constraint.bottom() - pinPoint.y()) / (result.bottom() - pinPoint.y());
            }

            qreal scaleFactor = qMin(xScaleFactor, yScaleFactor);

            if (scaleFactor < 0 || qFuzzyIsNull(scaleFactor)) {
                result = QnGeometry::movedInto(result, overlayWidget->rect());
            } else {
                result = ConstrainedResizable::constrainedGeometry(result, pinSection, pinPoint, result.size() * scaleFactor);
            }
        } else {
            result = QnGeometry::movedInto(result, overlayWidget->rect());
        }
    }

    return result;
}



// -------------------------------------------------------------------------- //
// ZoomWindowInstrument
// -------------------------------------------------------------------------- //
ZoomWindowInstrument::ZoomWindowInstrument(QObject *parent):
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
    m_zoomWindowStartedEmitted(false),
    m_resizingInstrument(nullptr),
    m_selectionItem(nullptr),
    m_viewport(nullptr),
    m_target(nullptr),
    m_windowTarget(nullptr),
    m_storedWindowWidget(nullptr)
{
    dragProcessor()->setStartDragTime(0);

    /* Sensible default. */
    m_colors
        << qnGlobals->warningTextColor()
        << qnGlobals->successTextColor()
        << qnGlobals->errorTextColor();

    connect(display(), &QnWorkbenchDisplay::zoomLinkAdded,              this, &ZoomWindowInstrument::at_display_zoomLinkAdded);
    connect(display(), &QnWorkbenchDisplay::zoomLinkAboutToBeRemoved,   this, &ZoomWindowInstrument::at_display_zoomLinkAboutToBeRemoved);
    connect(display(), &QnWorkbenchDisplay::widgetChanged,              this, &ZoomWindowInstrument::at_display_widgetChanged);
}

ZoomWindowInstrument::~ZoomWindowInstrument() {
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
    QSet<QColor> colors;
    foreach(QnResourceWidget *widget, display()->widgets())
        colors.insert(widget->frameDistinctionColor());

    foreach(const QColor &color, m_colors)
        if(!colors.contains(color))
            return color;

    if(m_colors.isEmpty()) {
        return Qt::white;
    } else {
        return m_colors[nx::utils::random::number(0, m_colors.size() - 1)];
    }
}

ZoomOverlayWidget *ZoomWindowInstrument::overlayWidget(QnMediaResourceWidget *widget) const {
    return m_dataByWidget[widget].overlayWidget;
}

ZoomOverlayWidget *ZoomWindowInstrument::ensureOverlayWidget(QnMediaResourceWidget *widget) {
    ZoomData &data = m_dataByWidget[widget];

    ZoomOverlayWidget *overlayWidget = data.overlayWidget;
    if(overlayWidget)
        return overlayWidget;

    overlayWidget = new ZoomOverlayWidget();
    overlayWidget->setOpacity(1.0);
    overlayWidget->setTarget(widget);
    data.overlayWidget = overlayWidget;

    widget->addOverlayWidget(overlayWidget, QnResourceWidget::UserVisible);
    updateOverlayMode(widget);

    return overlayWidget;
}

ZoomWindowWidget *ZoomWindowInstrument::windowWidget(QnMediaResourceWidget *widget) const {
    return m_dataByWidget[widget].windowWidget;
}

QnMediaResourceWidget *ZoomWindowInstrument::target() const {
    return m_target.data();
}

ZoomWindowWidget *ZoomWindowInstrument::windowTarget() const {
    return m_windowTarget.data();
}

FixedArSelectionItem *ZoomWindowInstrument::selectionItem() const {
    return m_selectionItem.data();
}

void ZoomWindowInstrument::ensureSelectionItem() {
    if(selectionItem())
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

void ZoomWindowInstrument::registerWidget(QnMediaResourceWidget *widget) {
    connect(widget, &QnResourceWidget::zoomRectChanged,                 this, &ZoomWindowInstrument::at_widget_zoomRectChanged);
    connect(widget, &QnResourceWidget::frameDistinctionColorChanged,    this, &ZoomWindowInstrument::at_widget_frameColorChanged);
    connect(widget, &QnResourceWidget::aboutToBeDestroyed,              this, &ZoomWindowInstrument::at_widget_aboutToBeDestroyed);
    connect(widget, &QnResourceWidget::optionsChanged,                  this, &ZoomWindowInstrument::at_widget_optionsChanged);

    /* Initialize frame color if zoom window was loaded from a layout and
     * not created through this instrument. */
    if(!widget->zoomRect().isNull() && !widget->frameDistinctionColor().isValid())
        widget->setFrameDistinctionColor(nextZoomWindowColor());
}

void ZoomWindowInstrument::unregisterWidget(QnMediaResourceWidget *widget) {
    if(!m_dataByWidget.contains(widget))
        return; /* Double unregistration is possible and is OK. */

    disconnect(widget, NULL, this, NULL);

    m_dataByWidget.remove(widget);

    if (m_target == widget)
    {
        m_target->unsetCursor();
        m_target = nullptr;
    }
}

void ZoomWindowInstrument::registerLink(QnMediaResourceWidget *widget, QnMediaResourceWidget *zoomTargetWidget)
{
    ZoomData &data = m_dataByWidget[widget];

    ZoomOverlayWidget *overlayWidget = ensureOverlayWidget(zoomTargetWidget);

    ZoomWindowWidget *windowWidget;
    if(m_storedWindowWidget) {
        windowWidget = m_storedWindowWidget.data();
        m_storedWindowWidget.clear();
        windowWidget->show();
    } else {
        windowWidget = new ZoomWindowWidget();
    }
    windowWidget->setZoomWidget(widget);
    windowWidget->setFrameWidth(kZoomLineWidth);
    overlayWidget->addWidget(windowWidget);
    data.windowWidget = windowWidget;
    connect(windowWidget, &ZoomWindowWidget::geometryChanged,   this, &ZoomWindowInstrument::at_windowWidget_geometryChanged);
    connect(windowWidget, &ZoomWindowWidget::doubleClicked,     this, &ZoomWindowInstrument::at_windowWidget_doubleClicked);

    updateWindowFromWidget(widget);
}

void ZoomWindowInstrument::unregisterLink(QnMediaResourceWidget *widget, QnMediaResourceWidget *zoomTargetWidget, bool deleteWindowWidget) {
    Q_UNUSED(zoomTargetWidget)
    ZoomData &data = m_dataByWidget[widget];

    if(deleteWindowWidget) {
        delete data.windowWidget;
    } else {
        data.windowWidget->overlay()->removeWidget(data.windowWidget);
        data.windowWidget->setZoomWidget(NULL);
        disconnect(data.windowWidget, NULL, this, NULL);
    }
    data.windowWidget = NULL;
}

void ZoomWindowInstrument::updateOverlayMode(QnMediaResourceWidget *widget) {
    ZoomOverlayWidget *overlayWidget = this->overlayWidget(widget);
    if(!overlayWidget)
        return;

    qreal opacity = 0.0;
    bool interactive = false;
    bool instant = false;
    if(widget == display()->widget(Qn::ZoomedRole)) {
        /* Leave invisible. */
    } else if(widget->options() & QnResourceWidget::DisplayDewarped) {
        /* Leave invisible. */
        instant = true;
    } else if(widget->options() & (QnResourceWidget::DisplayMotion | QnResourceWidget::DisplayMotionSensitivity)) {
        /* Leave invisible. */
    } else if(widget->options() & QnResourceWidget::DisplayCrosshair) {
        /* PTZ mode - transparent, non-interactive. */
        opacity = 0.3;
    } else {
        opacity = 1.0;
        interactive = true;
    }

    if(instant) {
        opacityAnimator(overlayWidget, 1.0)->stop();
        overlayWidget->setOpacity(opacity);
    } else {
        opacityAnimator(overlayWidget, 1.0)->animateTo(opacity);
    }
    overlayWidget->setInteractive(interactive);
}

void ZoomWindowInstrument::updateWindowFromWidget(QnMediaResourceWidget *widget) {
    if(m_processingWidgets.contains(widget))
        return;
    m_processingWidgets.insert(widget);

    ZoomWindowWidget *windowWidget = this->windowWidget(widget);
    ZoomOverlayWidget *overlayWidget = windowWidget ? windowWidget->overlay() : NULL;

    if(windowWidget && overlayWidget) {
        overlayWidget->setWidgetRect(windowWidget, widget->zoomRect(), true);
        windowWidget->setFrameColor(widget->frameDistinctionColor().lighter(110));
    }

    m_processingWidgets.remove(widget);
}

void ZoomWindowInstrument::updateWidgetFromWindow(ZoomWindowWidget *windowWidget) {
    ZoomOverlayWidget *overlayWidget = windowWidget->overlay();
    QnMediaResourceWidget *zoomWidget = windowWidget->zoomWidget();

    if(m_processingWidgets.contains(zoomWidget))
        return;
    m_processingWidgets.insert(zoomWidget);

    if(overlayWidget && zoomWidget && !qFuzzyIsNull(overlayWidget->size())) {
        QRectF zoomRect = cwiseDiv(windowWidget->geometry(), overlayWidget->size());
        overlayWidget->setWidgetRect(windowWidget, zoomRect, false);
        emit zoomRectChanged(zoomWidget, zoomRect);
    }

    m_processingWidgets.remove(zoomWidget);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void ZoomWindowInstrument::installedNotify() {
    NX_ASSERT(selectionItem() == NULL);

    m_resizingInstrument = manager()->instrument<ResizingInstrument>();
    if(m_resizingInstrument) {
        connect(m_resizingInstrument,   &ResizingInstrument::resizingStarted,   this, &ZoomWindowInstrument::at_resizingStarted);
        connect(m_resizingInstrument,   &ResizingInstrument::resizing,          this, &ZoomWindowInstrument::at_resizing);
        connect(m_resizingInstrument,   &ResizingInstrument::resizingFinished,  this, &ZoomWindowInstrument::at_resizingFinished);
    }

    base_type::installedNotify();
}

void ZoomWindowInstrument::aboutToBeUninstalledNotify() {
    base_type::aboutToBeUninstalledNotify();

    if(ResizingInstrument *resizingInstrument = manager()->instrument<ResizingInstrument>())
        disconnect(resizingInstrument, NULL, this, NULL);

    if(selectionItem())
        delete selectionItem();
}

bool ZoomWindowInstrument::registeredNotify(QGraphicsItem *item) {
    if(!base_type::registeredNotify(item))
        return false;

    if(QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(item)) {
        registerWidget(widget);
        return true;
    }

    return false;
}

void ZoomWindowInstrument::unregisteredNotify(QGraphicsItem *item) {
    base_type::unregisteredNotify(item);

    /* Note that this dynamic_cast may fail because media resource widget is
     * already being destroyed. We don't care as in this case it will already
     * be unregistered in aboutToBeDestroyed handler. */
    if(QnMediaResourceWidget *widget = dynamic_cast<QnMediaResourceWidget *>(item))
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

void ZoomWindowInstrument::startDragProcess(DragInfo *) {
    emit zoomWindowProcessStarted(target());
}

void ZoomWindowInstrument::startDrag(DragInfo *) {
    m_zoomWindowStartedEmitted = false;

    if(!target()) {
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

    /* Everything else will be initialized in the first call to drag(). */

    emit zoomWindowStarted(target());
    m_zoomWindowStartedEmitted = true;
}

void ZoomWindowInstrument::dragMove(DragInfo *info) {
    if(!target()) {
        reset();
        return;
    }

    qreal originalAr = aspectRatio(target()->size()) / aspectRatio(target()->channelLayout()->size());

    // Here are the special algorithm by #rvasilenko
    int resizeCoef = 1;
    qreal targetAr = originalAr;
    while (targetAr > kMaxZoomWindowAr)
    {
        ++resizeCoef;
        targetAr = originalAr / resizeCoef;
    }

    ensureSelectionItem();
    selectionItem()->setGeometry(info->mousePressItemPos(), info->mouseItemPos(), targetAr, target()->rect());
}

void ZoomWindowInstrument::finishDrag(DragInfo *) {
    if(target())
    {
        ensureSelectionItem();
        opacityAnimator(selectionItem(), 4.0)->animateTo(0.0);

        QRectF zoomRect = cwiseDiv(selectionItem()->rect(), target()->size());
        if (qFuzzyIsNull(zoomRect.width()))
            zoomRect.setWidth(kZoomWindowMinSize);
        if (qFuzzyIsNull(zoomRect.height()))
            zoomRect.setHeight(kZoomWindowMinSize);

        qreal ar = aspectRatio(zoomRect);
        ar = qBound(kZoomWindowMinAspectRatio, ar, kZoomWindowMaxAspectRatio);

        if (zoomRect.width() < kZoomWindowMinSize || zoomRect.height() < kZoomWindowMinSize)
        {
            const QSizeF minSize(kZoomWindowMinSize, kZoomWindowMinSize);
            zoomRect = expanded(ar, minSize, zoomRect.center(), Qt::KeepAspectRatioByExpanding);
        }
        else if (zoomRect.width() > kZoomWindowMaxSize || zoomRect.height() > kZoomWindowMaxSize)
        {
            const QSizeF maxSize(kZoomWindowMaxSize, kZoomWindowMaxSize);
            zoomRect = expanded(ar, maxSize, zoomRect.center(), Qt::KeepAspectRatio);
        }

        // Coordinates are relative to source rect
        zoomRect = movedInto(zoomRect, QRectF(0.0, 0.0, 1.0, 1.0));

        emit zoomRectCreated(target(), m_zoomWindowColor, zoomRect);
    }

    if(m_zoomWindowStartedEmitted)
        emit zoomWindowFinished(target());
}

void ZoomWindowInstrument::finishDragProcess(DragInfo *) {
    emit zoomWindowProcessFinished(target());
}

void ZoomWindowInstrument::at_widget_aboutToBeDestroyed() {
    unregisterWidget(checked_cast<QnMediaResourceWidget *>(sender()));
}

void ZoomWindowInstrument::at_widget_zoomRectChanged() {
    updateWindowFromWidget(checked_cast<QnMediaResourceWidget *>(sender()));
}

void ZoomWindowInstrument::at_widget_frameColorChanged() {
    updateWindowFromWidget(checked_cast<QnMediaResourceWidget *>(sender()));
}

void ZoomWindowInstrument::at_widget_optionsChanged() {
    updateOverlayMode(checked_cast<QnMediaResourceWidget *>(sender()));
}

void ZoomWindowInstrument::at_windowWidget_geometryChanged() {
    updateWidgetFromWindow(checked_cast<ZoomWindowWidget *>(sender()));
}

void ZoomWindowInstrument::at_windowWidget_doubleClicked() {
    ZoomWindowWidget *windowWidget = checked_cast<ZoomWindowWidget *>(sender());

    // TODO: #Elric does not belong here.
    QnMediaResourceWidget *zoomWidget = windowWidget->zoomWidget();
    if(zoomWidget)
        workbench()->setItem(Qn::ZoomedRole, zoomWidget->item());
}

void ZoomWindowInstrument::at_display_widgetChanged(Qn::ItemRole role) {
    if(role != Qn::ZoomedRole)
        return;

    if(m_zoomedWidget)
        updateOverlayMode(m_zoomedWidget.data());

    m_zoomedWidget = dynamic_cast<QnMediaResourceWidget *>(display()->widget(role));

    if(m_zoomedWidget)
        updateOverlayMode(m_zoomedWidget.data());
}

void ZoomWindowInstrument::at_display_zoomLinkAdded(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget) {
    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
    QnMediaResourceWidget *mediaZoomTargetWidget = dynamic_cast<QnMediaResourceWidget *>(zoomTargetWidget);

    if(mediaWidget && mediaZoomTargetWidget)
        registerLink(mediaWidget, mediaZoomTargetWidget);
}

void ZoomWindowInstrument::at_display_zoomLinkAboutToBeRemoved(QnResourceWidget *widget, QnResourceWidget *zoomTargetWidget) {
    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
    QnMediaResourceWidget *mediaZoomTargetWidget = dynamic_cast<QnMediaResourceWidget *>(zoomTargetWidget);

    if(mediaWidget && mediaZoomTargetWidget)
        unregisterLink(mediaWidget, mediaZoomTargetWidget);
}

void ZoomWindowInstrument::at_resizingStarted(QGraphicsView *, QGraphicsWidget *widget, ResizingInfo *info) {
    if(info->frameSection() != Qt::TitleBarArea)
        return;

    m_windowTarget = dynamic_cast<ZoomWindowWidget *>(widget);
}

void ZoomWindowInstrument::at_resizing(QGraphicsView *view, QGraphicsWidget *, ResizingInfo *info) {
    if(!windowTarget() || windowTarget() == m_storedWindowWidget.data())
        return;

    QnMediaResourceWidget *newTargetWidget = this->item<QnMediaResourceWidget>(view, info->mouseViewportPos());
    if(!newTargetWidget || newTargetWidget == windowTarget()->overlay()->target())
        return;

    auto action = menu()->action(action::CreateZoomWindowAction);
    if (!action || action->checkCondition(action->scope(), newTargetWidget) != action::EnabledAction)
        return;

    QnMediaResourceWidget *widget = windowTarget()->zoomWidget();
    QnMediaResourceWidget *oldTargetWidget = windowTarget()->overlay()->target();

    /* Preserve zoom window widget. */
    windowTarget()->hide();
    unregisterLink(widget, oldTargetWidget, false);

    if(m_storedWindowWidget)
        delete m_storedWindowWidget.data(); /* Just to feel safe that we don't leak memory. */
    m_storedWindowWidget = windowTarget();

    QRectF zoomRect = widget->zoomRect();
    QSizeF oldLayoutSize = oldTargetWidget->channelLayout()->size();
    QSizeF newLayoutSize = newTargetWidget->channelLayout()->size();
    if(oldLayoutSize != newLayoutSize) {
        QSizeF zoomSize = cwiseDiv(cwiseMul(zoomRect.size(), oldLayoutSize), newLayoutSize);
        zoomRect = movedInto(QRectF(zoomRect.topLeft(), zoomSize), QRectF(0.0, 0.0, 1.0, 1.0));
    }

    emit zoomTargetChanged(widget, zoomRect, newTargetWidget);
    m_resizingInstrument->rehandle();
}

void ZoomWindowInstrument::at_resizingFinished(QGraphicsView *, QGraphicsWidget *, ResizingInfo *) {
    m_windowTarget.clear();
}


