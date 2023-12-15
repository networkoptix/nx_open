// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "navigation_item.h"

#include <QtCore/QSizeF>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QGraphicsSceneWheelEvent>

#include <qt_graphics_items/graphics_widget.h>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/icon.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/skin/svg_icon_colorer.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/utils/context_utils.h>
#include <nx/vms/client/desktop/workbench/timeline/thumbnail_panel.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/generic/framed_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/separator.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/scoped_painter_rollback.h>

#include "time_scroll_bar.h"
#include "time_slider.h"
#include "timeline_placeholder.h"

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

namespace {

static constexpr qreal kScrollBarHeight = 16;
static const qreal kHeight = QnTimeSlider::expectedHeight() + kScrollBarHeight;
static constexpr qreal kDarkSeparatorThickness = 1;
static constexpr qreal kLightSeparatorThickness = 1;
static constexpr QSizeF kZoomButtonSize = {38, 42};

} // namespace

QnNavigationItem::QnNavigationItem(QGraphicsItem *parent):
    base_type(parent),
    WindowContextAware(utils::windowContextFromObject(parent->toGraphicsObject()))
{
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    setFlag(QGraphicsItem::ItemIsFocusable, true);
    setFocusPolicy(Qt::ClickFocus);
    setCursor(Qt::ArrowCursor);

    setAutoFillBackground(true);
    setPaletteColor(this, QPalette::Window, core::colorTheme()->color("dark6"));
    setPaletteColor(this, QPalette::Shadow, core::colorTheme()->color("dark1", 38));

    m_navigationWidgetPlaceholder = new GraphicsWidget(this);
    m_controlWidgetPlaceholder = new GraphicsWidget(this);
    connect(this, &GraphicsWidget::geometryChanged, this,
        [this]
        {
            emitNavigationGeometryChanged();
            emitControlGeometryChanged();
        });
    connect(m_navigationWidgetPlaceholder, &GraphicsWidget::geometryChanged,
        this, &QnNavigationItem::emitNavigationGeometryChanged);
    connect(m_controlWidgetPlaceholder, &GraphicsWidget::geometryChanged,
        this, &QnNavigationItem::emitControlGeometryChanged);

    m_timeSlider = new QnTimeSlider(windowContext(), this, parent);
    m_timeSlider->setOption(QnTimeSlider::UnzoomOnDoubleClick, false);

    GraphicsLabel* timelinePlaceholder = new QnTimelinePlaceholder(this, m_timeSlider);
    timelinePlaceholder->setPerformanceHint(GraphicsLabel::PixmapCaching);

    m_timeScrollBar = new QnTimeScrollBar(this);
    m_timeScrollBar->setPreferredHeight(kScrollBarHeight);

    m_sliderInnerShading = new QnFramedWidget(this);
    m_sliderInnerShading->setWindowBrush(Qt::NoBrush);
    m_sliderInnerShading->setFrameShape(Qn::RectangularFrame);
    m_sliderInnerShading->setFrameBorders(Qt::LeftEdge | Qt::RightEdge);
    m_sliderInnerShading->setFrameColor(palette().color(QPalette::Shadow));

    const QColor darkSeparatorColor = core::colorTheme()->color("dark5");
    const QColor lightSeparatorColor = core::colorTheme()->color("dark7");
    const SeparatorAreas separatorAreas = {
        { kDarkSeparatorThickness, darkSeparatorColor },
        { kLightSeparatorThickness, lightSeparatorColor }};

    // Construct timeline zoom buttons.
    auto zoomButtonsLayout = new QGraphicsLinearLayout(Qt::Vertical);
    zoomButtonsLayout->setSpacing(0);
    zoomButtonsLayout->setContentsMargins(0, 0, 0, 0);

    createZoomButton(Zooming::In, "In", "slider_zoom_in", zoomButtonsLayout);
    auto separatorBetweenZoomButtons = new QnSeparator(Qt::Horizontal, separatorAreas, this);
    separatorBetweenZoomButtons->setMaximumWidth(kZoomButtonSize.width());
    zoomButtonsLayout->addItem(separatorBetweenZoomButtons);
    createZoomButton(Zooming::Out, "Out", "slider_zoom_out", zoomButtonsLayout);

    auto zoomButtons = new GraphicsWidget(this);
    zoomButtons->setLayout(zoomButtonsLayout);

    auto separatorBeforeSlider = new QnSeparator(Qt::Vertical, 1, lightSeparatorColor, this);
    auto separatorAfterSlider = new QnSeparator(Qt::Vertical, 1, darkSeparatorColor, this);

    connect(navigator(), &QnWorkbenchNavigator::timelineRelevancyChanged, this,
        [this, zoomButtons, separatorBeforeSlider, separatorAfterSlider, timelinePlaceholder]
            (bool isRelevant)
        {
            bool reset = m_timeSlider->isVisible() != isRelevant;
            m_timeSlider->setVisible(isRelevant);
            m_timeScrollBar->setVisible(isRelevant);
            zoomButtons->setEnabled(isRelevant);
            separatorBeforeSlider->setVisible(!isRelevant);
            separatorAfterSlider->setVisible(!isRelevant);
            timelinePlaceholder->setVisible(!isRelevant);
            m_sliderInnerShading->setFrameWidth(isRelevant ? 2.0 : 0.0);
            if (reset)
                m_timeSlider->invalidateWindow();
        });

    /* Initialize navigator. */
    navigator()->setTimeSlider(m_timeSlider);
    navigator()->setTimeScrollBar(m_timeScrollBar);

    /* Put it all into layouts. */
    auto navigationLayout = new QGraphicsLinearLayout(Qt::Vertical);
    navigationLayout->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    navigationLayout->setContentsMargins(6, 0, 6, 0);
    navigationLayout->setSpacing(0);
    navigationLayout->setMinimumHeight(kHeight - kLightSeparatorThickness);
    navigationLayout->addItem(m_navigationWidgetPlaceholder);

    auto navigationPanel = new GraphicsWidget(this);
    navigationPanel->setLayout(navigationLayout);

    auto leftLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    leftLayout->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);
    leftLayout->addItem(navigationPanel);
    leftLayout->addItem(new QnSeparator(Qt::Vertical, separatorAreas, this));
    leftLayout->addItem(zoomButtons);
    leftLayout->setAlignment(zoomButtons, Qt::AlignCenter);

    auto leftPanel = new QnFramedWidget(this);
    leftPanel->setFrameShape(Qn::NoFrame);
    leftPanel->setLayout(leftLayout);

    auto rightLayout = new QGraphicsLinearLayout(Qt::Vertical);
    rightLayout->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    rightLayout->setContentsMargins(6, 1 + kLightSeparatorThickness, 6, 6);
    rightLayout->setSpacing(2);
    rightLayout->setMinimumHeight(kHeight - kLightSeparatorThickness);
    rightLayout->addItem(m_controlWidgetPlaceholder);

    auto rightPanel = new QnFramedWidget(this);
    rightPanel->setFrameShape(Qn::NoFrame);
    rightPanel->setLayout(rightLayout);

    m_sliderLayout = new QGraphicsLinearLayout(Qt::Vertical);
    m_sliderLayout->setContentsMargins(0, 0, 0, 0);
    m_sliderLayout->setSpacing(0);
    m_sliderLayout->addItem(m_timeSlider);
    m_sliderLayout->addItem(m_timeScrollBar);
    m_sliderLayout->addItem(timelinePlaceholder);

    m_timeSlider->hide();
    m_timeScrollBar->hide();

    auto bottomLayout = new QGraphicsLinearLayout(Qt::Horizontal);
    bottomLayout->setContentsMargins(0, kDarkSeparatorThickness, 0, 0);
    bottomLayout->setSpacing(0);
    bottomLayout->setMinimumHeight(kHeight + kDarkSeparatorThickness);
    bottomLayout->addItem(leftPanel);
    bottomLayout->addItem(new QnSeparator(Qt::Vertical, 1, darkSeparatorColor, this));
    bottomLayout->addItem(separatorBeforeSlider);
    bottomLayout->addItem(m_sliderLayout);
    bottomLayout->addItem(separatorAfterSlider);
    bottomLayout->addItem(new QnSeparator(Qt::Vertical, 1, lightSeparatorColor, this));
    bottomLayout->addItem(rightPanel);
    bottomLayout->setAlignment(rightPanel, Qt::AlignLeft | Qt::AlignBottom);

    auto bottomPanel = new QnFramedWidget(this);
    bottomPanel->setFrameBorders(Qt::TopEdge);
    bottomPanel->setFrameColor(lightSeparatorColor);
    bottomPanel->setFrameWidth(kDarkSeparatorThickness);
    bottomPanel->setLayout(bottomLayout);
    m_sliderInnerShading->setParentItem(bottomPanel);

    m_thumbnailPanel = new ThumbnailPanel(m_timeSlider, this);
    m_timeSlider->setThumbnailPanel(m_thumbnailPanel);

    auto mainLayout = new QGraphicsLinearLayout(Qt::Vertical);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addItem(m_thumbnailPanel);
    mainLayout->addItem(bottomPanel);
    setLayout(mainLayout);

    /* Set up handlers. */

    connect(this, &QGraphicsWidget::geometryChanged, this, &QnNavigationItem::updateShadingPos);
    connect(this, &QGraphicsWidget::visibleChanged, this, &QnNavigationItem::updateShadingPos);
    connect(timelinePlaceholder, &QGraphicsWidget::geometryChanged,
        this, &QnNavigationItem::updateShadingPos);

    connect(navigator(), &QnWorkbenchNavigator::liveSupportedChanged,
        this, [this]() { m_timeSlider->setLiveSupported(navigator()->isLiveSupported()); });
    connect(navigator(), &QnWorkbenchNavigator::liveChanged, this,
        [this] { m_timeSlider->finishAnimations(); });

    updateGeometry();
}

void QnNavigationItem::createZoomButton(
    Zooming zooming,
    const QString& zoomingName,
    const QString& statisticsName,
    QGraphicsLinearLayout* layout)
{
    auto button = new QnImageButtonWidget();
    button->setObjectName(QString("TimelineZoom%1Button").arg(zoomingName));

    const auto fileName = QString("slider/buttons/zoom_%1.svg").arg(zoomingName.toLower());

    const QColor background = "#1c2327";
    const QColor foreground = "#698796";
    const core::SvgIconColorer::IconSubstitutions substitutions = {
        { QnIcon::Normal, {{background, "dark6"}, {foreground, "light16"}}},
        { QnIcon::Active, {{background, "dark7"}, {foreground, "light16"}}},
        { QnIcon::Pressed, {{background, "dark5"}, {foreground, "light16"}}},
        { QnIcon::Disabled, {{background, "dark6"}, {foreground, "dark9"}}}
    };

    button->setIcon(qnSkin->icon(fileName, "", nullptr, substitutions));
    button->setPreferredSize(kZoomButtonSize);
    statisticsModule()->controls()->registerButton(
        statisticsName,
        button);

    connect(button, &QnImageButtonWidget::pressed, this,
        [this, zooming]() { m_zooming = zooming; });

    connect(button, &QnImageButtonWidget::released, this,
        [this]()
        {
            m_zooming = Zooming::None;
            m_timeSlider->hurryKineticAnimations();
        });

    layout->addItem(button);
}

QnTimeSlider* QnNavigationItem::timeSlider() const
{
    return m_timeSlider;
}

QnNavigationItem::ThumbnailPanel* QnNavigationItem::thumbnailPanel() const
{
    return m_thumbnailPanel;
}

void QnNavigationItem::setNavigationRectSize(const QRectF& rect)
{
    m_navigationWidgetPlaceholder->setMinimumWidth(rect.width());
    m_navigationWidgetPlaceholder->setMinimumHeight(rect.height());
    updateGeometry();
}

void QnNavigationItem::setControlRectSize(const QRectF& rect)
{
    m_controlWidgetPlaceholder->setMinimumWidth(rect.width());
    m_controlWidgetPlaceholder->setMinimumHeight(rect.height());
    updateGeometry();
}

QnNavigationItem::Zooming QnNavigationItem::zooming() const
{
    return m_zooming;
}

void QnNavigationItem::updateShadingPos()
{
    m_sliderInnerShading->setGeometry(m_sliderLayout->geometry());
}

QRectF QnNavigationItem::widgetGeometryMappedToThis(const GraphicsWidget* widget) const
{
    return widget->parentWidget()->mapRectToItem(this, widget->geometry());
}

void QnNavigationItem::emitNavigationGeometryChanged()
{
    emit navigationPlaceholderGeometryChanged(
        widgetGeometryMappedToThis(m_navigationWidgetPlaceholder));
}

void QnNavigationItem::emitControlGeometryChanged()
{
    emit controlPlaceholderGeometryChanged(
        widgetGeometryMappedToThis(m_controlWidgetPlaceholder));
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnNavigationItem::eventFilter(QObject *watched, QEvent *event)
{
    return base_type::eventFilter(watched, event);
}

void QnNavigationItem::wheelEvent(QGraphicsSceneWheelEvent *event)
{
    base_type::wheelEvent(event);
    event->accept(); /* Don't let wheel events escape into the scene. */
}

void QnNavigationItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    base_type::mousePressEvent(event);
    event->accept(); /* Prevent surprising click-through scenarios. */
}

bool QnNavigationItem::isTimelineRelevant() const
{
    return this->navigator() && this->navigator()->isTimelineRelevant();
}

void QnNavigationItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    /* Time slider and time scrollbar has own backgrounds.
     * Subtract them for proper semi-transparency: */
    QRegion clipRegion(rect().toRect());
    if (m_timeSlider->isVisible())
        clipRegion -= m_timeSlider->geometry().toRect();
    if (m_timeScrollBar->isVisible())
        clipRegion -= m_timeScrollBar->geometry().toRect();

    QnScopedPainterClipRegionRollback clipRollback(painter, clipRegion);
    base_type::paint(painter, option, widget);
}
