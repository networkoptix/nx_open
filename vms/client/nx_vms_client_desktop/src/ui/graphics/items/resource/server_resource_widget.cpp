// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_resource_widget.h"

#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QStyleOptionGraphicsItem>

#include <qt_graphics_items/graphics_label.h>

#include <api/media_server_statistics_manager.h>
#include <client/client_runtime_settings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource_display_info.h>
#include <nx/utils/math/math.h> /* For M_PI. */
#include <nx/utils/string.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/license/videowall_license_validator.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/license/usage_helper.h>
#include <nx/vms/text/human_readable.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/common/palette.h>
#include <ui/graphics/items/generic/image_button_bar.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/viewport_bound_widget.h>
#include <ui/graphics/items/overlays/resource_title_item.h>
#include <ui/graphics/items/resource/button_ids.h>
#include <ui/graphics/opengl/gl_context_data.h>
#include <ui/graphics/opengl/gl_shortcuts.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_item.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/math/color_transformations.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

namespace {
    /** Convert angle from radians to degrees */
    qreal radiansToDegrees(qreal radian) {
        return (180 * radian) / M_PI;
    }

    /** Create path for the chart */
    QPainterPath createChartPath(
        const QList<qreal> &values, qreal x_step, qreal scale, qreal elapsedStep, qreal *currentValue)
    {
        QPainterPath path;
        qreal maxValue = -1;
        qreal lastValue = 0;
        const qreal x_step2 = x_step*.5;

        qreal x1 = -x_step * elapsedStep;
        qreal y1 = 0.0;

        qreal baseAngle = elapsedStep >= 0.5
            ? radiansToDegrees(qAcos(2 * (1 - elapsedStep)))
            : radiansToDegrees(qAcos(2 * elapsedStep));

        bool first = true;
        bool last = false;
        *currentValue = -1;

        auto backPos = values.cend();
        backPos--;

        for (auto pos = values.cbegin(); pos != values.cend(); pos++) {
            qreal value = qMin(*pos, 1.0);
            value = qMax(value, 0.0);
            last = pos == backPos;
            maxValue = qMax(maxValue, value);
            if (first) {
                y1 = value * scale;
                path.moveTo(x1, y1);
                first = false;
                continue;
            }

            /* Drawing only second part of the arc, cut at the beginning */
            bool c1 = x1 + x_step2 < 0.0;

            /* Drawing both parts of the arc, cut at the beginning */
            bool c2 = !c1 && x1 < 0.0;

            /* Drawing both parts of the arc as usual */
            bool c3 = !c1 && !c2 && !last;

            /* Drawing both parts of the arc, cut at the end */
            bool c4 = !c1 && !c2 && !c3 && elapsedStep >= 0.5;

            /* Drawing only first part of the arc, cut at the end */
            bool c5 = !c1 && !c2 && !c3 && !c4;

            qreal y2 = value * scale;
            if (y2 != y1) {
                qreal h = qAbs(y2 - y1);
                qreal angle = (y2 > y1) ? baseAngle : -baseAngle;

                /* Note that we're using 89 degrees for arc length for a reason.
                 * When using full 90 degrees, arcs are connected, but Qt still
                 * inserts an empty line segment to join consecutive arcs.
                 * Path triangulator then chokes when trying to calculate a normal
                 * for this line segment, producing NaNs in its output.
                 * These NaNs are then fed to GPU, resulting in artifacts. */
                qreal a89 = (y2 > y1) ? 89 : -89;
                qreal a90 = (y2 > y1) ? 90 : -90;
                qreal y = qMin(y1, y2);

                if (c1)
                    path.arcMoveTo(x1 + x_step2, y, x_step, h, 180 + angle);
                if (c2)
                    path.arcMoveTo(x1 - x_step2, y, x_step, h, angle);
                if (c2 || c3 || c4)
                    path.arcTo(x1 - x_step2, y, x_step, h, c2 ? angle : a90, c2 ? -angle : -a89);
                if (c1 || c2 || c3 || c4)
                    path.arcTo(x1 + x_step2, y, x_step, h, c1 ? 180 + angle : 180, c1 ? a90 - angle : c4 ? angle : a89);
                if (c5)
                    path.arcTo(x1 - x_step2, y, x_step, h, a90, angle - a90);
            } else {
                if (c1 || c2)
                    path.moveTo(0.0, y1);
                if (c1 || c2 || c3)
                    path.lineTo(x1 + x_step, y2);
                if (c4 || c5)
                    path.lineTo(x1 + x_step * elapsedStep, y2);
            }

            if(last) {
                /* Value that will be shown */
                *currentValue = (lastValue * (1.0 - elapsedStep) + value * elapsedStep);
                break;
            }

            x1 += x_step;
            y1 = y2;
            lastValue = value;
        }

        return path;
    }
    /** Backward sorting because buttonBar inserts buttons in reversed order */
    bool statisticsDataLess(const QnStatisticsData &first, const QnStatisticsData &second) {
        if (first.deviceType == Qn::StatisticsNETWORK && second.deviceType == Qn::StatisticsNETWORK)
            return first.description.toLower() > second.description.toLower();

        if (first.deviceType != second.deviceType)
            return first.deviceType > second.deviceType;
        return first.description.toLower() > second.description.toLower();
    }

    const int legendImageSize = 20;
    const int legendFontSize = 20;
    const int legendMaxLength = 60;
    const int itemSpacing = 2;

    const char *legendKeyPropertyName = "_qn_legendKey";

} // namespace

// -------------------------------------------------------------------------- //
// LegendButtonWidget
// -------------------------------------------------------------------------- //
class LegendButtonWidget: public QnImageButtonWidget {
    typedef QnImageButtonWidget base_type;

public:
    LegendButtonWidget(const QString &text, const QColor &color, QGraphicsItem* parent = nullptr):
        base_type(parent),
        m_text(text),
        m_color(color)
    {
        setCheckable(true);
        setToolTip(m_text);
        setIcon(qnSkin->icon("health_monitor/check.png"));
    }

    virtual ~LegendButtonWidget() {}

    QString text() const {
        return m_text;
    }

    void setText(const QString &text) {
        if (m_text == text)
            return;
        m_text = text;
        update();
    }

    QColor color() const {
        return m_color;
    }

    void setColor(const QColor &color) {
        m_color = color;
        update();
    }

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override {
        switch (which) {
        case Qt::MinimumSize:
            return QSizeF(legendImageSize, legendImageSize + itemSpacing);
        case Qt::PreferredSize: {
            QFont font;
            font.setPixelSize(legendFontSize);
            return QSizeF(
                legendImageSize + QFontMetrics(font).horizontalAdvance(m_text) + itemSpacing * 2,
                legendImageSize + itemSpacing);
        }
        case Qt::MaximumSize: {
            QSizeF hint = base_type::sizeHint(which, constraint);
            return QSizeF(hint.width(), legendImageSize + itemSpacing);
        }
        default:
            break;
        }
        return base_type::sizeHint(which, constraint);
    }

    qreal stateOpacity(StateFlags stateFlags) {
        return (stateFlags & Hovered) ? 1.0 : 0.5;
    }

    virtual void paint(QPainter *painter, StateFlags startState, StateFlags endState, qreal progress, QOpenGLWidget *widget, const QRectF &rect) override
    {
        qreal opacity = painter->opacity();
        painter->setOpacity(opacity * (stateOpacity(startState) * (1.0 - progress) + stateOpacity(endState) * progress));

        QRectF imgRect = QRectF(0, 0, legendImageSize, legendImageSize);
        int textOffset = legendImageSize + itemSpacing;
        QRectF textRect = rect.adjusted(textOffset, 0, 0, 0);
        {
            // TODO: #sivanov Text drawing is very slow. Replace with cached textures where possible.
            QnScopedPainterPenRollback penRollback(painter,
                QPen(core::colorTheme()->color("dark1"), 2));
            QnScopedPainterBrushRollback brushRollback(painter);

            painter->setBrush(m_color);
            painter->drawRoundedRect(imgRect, 4, 4);

            QFont font;
            font.setPixelSize(legendFontSize);
            QnScopedPainterFontRollback fontRollback(painter, font);
            painter->setPen(QPen(m_color, 3));
            painter->drawText(textRect, m_text);
        }
        base_type::paint(painter, startState, endState, progress, widget, imgRect);
        painter->setOpacity(opacity);
    }

private:
    QString m_text;
    QColor m_color;
};


// -------------------------------------------------------------------------- //
// StatisticsOverlayWidget
// -------------------------------------------------------------------------- //
class StatisticsOverlayWidget: public GraphicsWidget {
    Q_DECLARE_TR_FUNCTIONS(StatisticsOverlayWidget)
    typedef GraphicsWidget base_type;
public:
    StatisticsOverlayWidget(QnServerResourceWidget *widget):
        base_type(widget),
        m_widget(widget)
    {}

    virtual ~StatisticsOverlayWidget() {}

protected:
    virtual QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override {
        return base_type::sizeHint(which == Qt::MinimumSize ? which : Qt::MaximumSize, constraint);
    }

    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) override {
        QnScopedPainterFontRollback fontRollback(painter);
        Q_UNUSED(fontRollback)
        QFont font(this->font());
        font.setPixelSize(legendFontSize);
        painter->setFont(font);

        QRectF rect = option->rect;

        qreal width = rect.width();
        qreal height = rect.height();

        bool isEmpty = m_widget->m_sortedKeys.isEmpty();

        qreal offsetX = isEmpty ? itemSpacing : itemSpacing * 2 + painter->fontMetrics().horizontalAdvance(QLatin1String("100%"));
        qreal offsetTop = isEmpty? itemSpacing : painter->fontMetrics().height() + itemSpacing;
        qreal offsetBottom = itemSpacing;

        const qreal grid_pen_width = 2.5;
        const qreal graph_pen_width = 3.0;
        const qreal frame_pen_width = 3.0;

        qreal ow = width - offsetX*2;
        qreal oh = height - offsetTop - offsetBottom;

        if (ow <= 0 || oh <= 0)
            return;

        QRectF inner(offsetX, offsetTop, ow, oh);

        qreal elapsed_step = m_widget->m_renderStatus == Qn::CannotRender ? 0 :
                (qreal)qBound((qreal)0, (qreal)m_widget->m_elapsedTimer.elapsed(), m_widget->m_updatePeriod) / m_widget->m_updatePeriod;

        const qreal x_step = (qreal)ow*1.0/(m_widget->m_pointsLimit - 2); // one point is cut from the beginning and one from the end
        const qreal y_step = oh * 0.025;

        /* Draw grid */
        {
            static const QColor kGridColor = core::colorTheme()->color("brand_core", 100);

            QPen grid;
            grid.setColor(kGridColor);
            grid.setWidthF(grid_pen_width);

            QPainterPath grid_path;
            for (qreal i = offsetX - (x_step * (elapsed_step + m_widget->m_counter%4 - 4)); i < ow + offsetX; i += x_step*4){
                grid_path.moveTo(i, offsetTop);
                grid_path.lineTo(i, oh + offsetTop);
            }
            for (qreal i = y_step*4 + offsetTop; i < oh + offsetTop; i += y_step*4){
                grid_path.moveTo(offsetX, i);
                grid_path.lineTo(ow + offsetX, i);
            }
            painter->strokePath(grid_path, grid);
        }

        QMap<QString, qreal> displayValues;

        /* Draw graph lines */
        {
            QnScopedPainterTransformRollback transformRollback(painter);
            Q_UNUSED(transformRollback)

            qreal space_offset = graph_pen_width;

            QTransform graphTransform = painter->transform();
            graphTransform.translate(offsetX, oh + offsetTop - space_offset);
            painter->setTransform(graphTransform);

            QPen graphPen;
            graphPen.setWidthF(graph_pen_width);
            graphPen.setCapStyle(Qt::FlatCap);

            foreach(QString key, m_widget->m_sortedKeys) {
                QnStatisticsData &stats = m_widget->m_history[key];

                const QnServerResourceWidget::GraphData &data = m_widget->m_graphDataByKey[key];
                if (!data.visible)
                    continue;

                const auto values = stats.values;

                qreal currentValue = 0;
                QPainterPath path = createChartPath(values, x_step, -1.0 * (oh - 2*space_offset), elapsed_step, &currentValue);
                displayValues[key] = currentValue;
                graphPen.setColor(toTransparent(data.color, data.opacity));
                painter->strokePath(path, graphPen);
            }
        }

        /* Draw frame and numeric values */
        {
            QnScopedPainterPenRollback penRollback(painter);
            Q_UNUSED(penRollback)

            static const QColor kFrameColor = core::colorTheme()->color("brand_core");

            QPen main_pen;
            main_pen.setColor(kFrameColor);
            main_pen.setWidthF(frame_pen_width);
            main_pen.setJoinStyle(Qt::MiterJoin);

            painter->setPen(main_pen);
            painter->drawRect(inner);

            /* Draw text values on the right side */
            {
                qreal opacity = painter->opacity();
                qreal baseOpacity = m_widget->isLegendVisible() ? 1.0 : 0.0;
                painter->setOpacity(opacity * baseOpacity);

                qreal xRight = offsetX + ow + itemSpacing * 2;
                foreach(QString key, m_widget->m_sortedKeys) {
                    const QnServerResourceWidget::GraphData &data = m_widget->m_graphDataByKey[key];
                    if (!data.visible)
                        continue;

                    qreal interValue = displayValues[key];
                    if (interValue < 0)
                        continue;
                    qreal y = offsetTop + qMax(offsetTop, oh * (1.0 - interValue));

                    main_pen.setColor(toTransparent(data.color, data.opacity));
                    painter->setPen(main_pen);
                    painter->drawText(xRight, y, QString("%1%").arg(qRound(interValue * 100.0)));
                }

                painter->setOpacity(opacity);
            }
        }
    }

private:
    QnServerResourceWidget *m_widget;
};

using HealthMonitoringButtons = QnServerResourceWidget::HealthMonitoringButtons;

// -------------------------------------------------------------------------- //
// QnServerResourceWidget
// -------------------------------------------------------------------------- //
QnServerResourceWidget::QnServerResourceWidget(
    SystemContext* systemContext,
    WindowContext* windowContext,
    QnWorkbenchItem* item,
    QGraphicsItem* parent)
    :
    base_type(systemContext, windowContext, item, parent),
    m_manager(systemContext->mediaServerStatisticsManager()),
    m_lastHistoryId(-1),
    m_counter(0),
    m_renderStatus(Qn::NothingRendered),
    m_hddCount(0),
    m_networkCount(0)
{
    connect(m_animationTimerListener.get(), &AnimationTimerListener::tick, this,
        &QnServerResourceWidget::tick);

    registerAnimation(m_animationTimerListener);
    m_animationTimerListener->startListening();

    setPaletteColor(this, QPalette::Window, core::colorTheme()->color("dark4"));

    m_resource = base_type::resource().dynamicCast<QnMediaServerResource>();
    NX_ASSERT(m_resource, "Server resource widget was created with a non-server resource.");
    m_manager->setFlagsFilter(
        Qn::StatisticsNETWORK, appContext()->localSettings()->statisticsNetworkFilter());
    m_pointsLimit = QnMediaServerStatisticsManager::kPointsLimit;
    m_manager->registerConsumer(m_resource, this, [this] { at_statistics_received(); });
    m_updatePeriod = m_manager->updatePeriod(m_resource);
    setOption(QnResourceWidget::WindowRotationForbidden);

    /* Note that this slot is already connected to nameChanged signal in
     * base class's constructor.*/
    connect(m_resource.get(), &QnResource::urlChanged,    this, &QnServerResourceWidget::updateTitleText);
    connect(m_resource.get(), &QnResource::statusChanged, this, &QnServerResourceWidget::updateTitleText);

    addOverlays();
    createButtons();

    /* Run handlers. */
    updateButtonsVisibility();
    updateTitleText();
    updateInfoText();
    at_statistics_received();
}

QnServerResourceWidget::~QnServerResourceWidget()
{
    m_manager->unregisterConsumer(m_resource, this);
}

QnMediaServerResourcePtr QnServerResourceWidget::resource() const {
    return m_resource;
}

QnServerResourceWidget::HealthMonitoringButtons QnServerResourceWidget::checkedHealthMonitoringButtons() const {
    HealthMonitoringButtons result;
    for(QHash<QString, GraphData>::const_iterator pos = m_graphDataByKey.constBegin(); pos != m_graphDataByKey.constEnd(); pos++) {
        GraphData data = *pos;
        result[pos.key()] = (data.bar->checkedButtons() & data.mask);
    }
    return result;
}

void QnServerResourceWidget::setCheckedHealthMonitoringButtons(const QnServerResourceWidget::HealthMonitoringButtons &buttons) {
    for(QHash<QString, GraphData>::iterator pos = m_graphDataByKey.begin(); pos != m_graphDataByKey.end(); pos++) {
        GraphData &data = *pos;
        QnImageButtonWidget* button = data.bar->button(data.mask);
        if (!button)
            continue;
        button->setChecked(buttons.value(pos.key(), true));
    }
    updateGraphVisibility();
}

QColor QnServerResourceWidget::getColor(Qn::StatisticsDeviceType deviceType, int index) {
    static const QList<QColor> kHddColors = core::colorTheme()->colors("server.statistics.hdds");
    static const QList<QColor> kNetworkColors = core::colorTheme()->colors("server.statistics.network");

    switch (deviceType) {
    case Qn::StatisticsCPU:
        return core::colorTheme()->color("server.statistics.cpu");
    case Qn::StatisticsRAM:
        return core::colorTheme()->color("server.statistics.ram");
    case Qn::StatisticsHDD:
        return kHddColors[qMod((qsizetype) index, kHddColors.size())];
    case Qn::StatisticsNETWORK:
        return kNetworkColors[qMod((qsizetype) index, kNetworkColors.size())];
    default:
        return QColor(Qt::white);
    }
}

int QnServerResourceWidget::helpTopicAt(const QPointF &) const {
    return HelpTopic::Id::MainWindow_MonitoringItem;
}

Qn::RenderStatus QnServerResourceWidget::paintChannelBackground(QPainter* painter, int channel,
    const QRectF& channelRect, const QRectF& paintRect)
{
    base_type::paintChannelBackground(painter, channel, channelRect, paintRect);
    return m_renderStatus;
}

void QnServerResourceWidget::addOverlays() {
    StatisticsOverlayWidget *statisticsOverlayWidget = new StatisticsOverlayWidget(this);
    statisticsOverlayWidget->setAcceptedMouseButtons(Qt::NoButton);

    QGraphicsLinearLayout *mainOverlayLayout = new QGraphicsLinearLayout(Qt::Vertical);
    mainOverlayLayout->setContentsMargins(0.5, 0.5, 0.5, 0.5);
    mainOverlayLayout->setSpacing(0.5);
    mainOverlayLayout->addItem(statisticsOverlayWidget);

    for (int i = 0; i < ButtonBarCount; i++) {
        m_legendButtonBar[i] = new QnImageButtonBar(this, {}, Qt::Horizontal);
        m_legendButtonBar[i]->setOpacity(0.0);

        connect(m_legendButtonBar[i], SIGNAL(checkedButtonsChanged()), this, SLOT(updateGraphVisibility()));

        QGraphicsLinearLayout *legendOverlayHLayout = new QGraphicsLinearLayout(Qt::Horizontal);
        legendOverlayHLayout->setContentsMargins(0.0, 0.0, 0.0, 0.0);
        legendOverlayHLayout->addStretch();
        legendOverlayHLayout->addItem(m_legendButtonBar[i]);
        legendOverlayHLayout->addStretch();
        mainOverlayLayout->addItem(legendOverlayHLayout);
    }

    QnViewportBoundWidget *mainOverlayWidget = new QnViewportBoundWidget(this);
    mainOverlayWidget->setLayout(mainOverlayLayout);
    mainOverlayWidget->setAcceptedMouseButtons(Qt::NoButton);
    mainOverlayWidget->setOpacity(1.0);
    addOverlayWidget(mainOverlayWidget, {UserVisible, OverlayFlag::autoRotate});
}

QnServerResourceWidget::LegendButtonBar QnServerResourceWidget::buttonBarByDeviceType(const Qn::StatisticsDeviceType deviceType) const {
    if(deviceType == Qn::StatisticsNETWORK) {
        return NetworkButtonBar;
    } else {
        return CommonButtonBar;
    }
}

void QnServerResourceWidget::updateLegend()
{
    const auto checkedData = savedCheckedHealthMonitoringButtons();

    QHash<Qn::StatisticsDeviceType, int> indexes;

    for (const QString& key: m_sortedKeys)
    {
        QnStatisticsData &stats = m_history[key];

        if (!m_graphDataByKey.contains(key)) {
            GraphData &data = m_graphDataByKey[key];
            data.bar = m_legendButtonBar[buttonBarByDeviceType(stats.deviceType)];
            data.mask = data.bar->unusedMask();
            data.visible = checkedData.value(key, true);
            data.color = getColor(stats.deviceType, indexes[stats.deviceType]++);

            LegendButtonWidget* newButton = new LegendButtonWidget(key, data.color);
            registerButtonStatisticsAlias(newButton, "server_resource_legend");
            newButton->setProperty(legendKeyPropertyName, key);
            newButton->setChecked(data.visible);

            connect(newButton, &QnImageButtonWidget::toggled, this,
                [this, key](bool toggled)
                {
                    if (!item())
                        return;

                    HealthMonitoringButtons value = savedCheckedHealthMonitoringButtons();
                    value[key] = toggled;
                    item()->setData(Qn::ItemHealthMonitoringButtonsRole,
                        QVariant::fromValue<HealthMonitoringButtons>(value));
                });

            connect(newButton,  &QnImageButtonWidget::stateChanged, this, &QnServerResourceWidget::updateHoverKey);

            { // fix text length on already existing buttons and the new one
                int mask = data.bar->visibleButtons();
                QList<LegendButtonWidget*> buttons;

                for (int i = 1; i <= mask && i > 0; i*=2)
                {
                    LegendButtonWidget* button = dynamic_cast<LegendButtonWidget*>(
                        data.bar->button(i));
                    if (!button)
                        continue;
                    buttons << button;
                }

                // we are adding one new button...
                int maxLength = qMax(legendMaxLength / (buttons.size() + 1), 2);

                foreach (LegendButtonWidget* button, buttons)
                {
                    QString text = button->property(legendKeyPropertyName).toString();
                    button->setText(nx::utils::elideString(text, maxLength));
                }

                if (key.length() > maxLength)
                    newButton->setText(nx::utils::elideString(key, maxLength));
            }

            data.button = newButton;
            data.bar->addButton(data.mask, data.button);

        }
    }
}

void QnServerResourceWidget::updateGraphVisibility() {
    for(QHash<QString, GraphData>::iterator pos = m_graphDataByKey.begin(); pos != m_graphDataByKey.end(); pos++) {
        GraphData &data = *pos;
        data.visible = data.bar->checkedButtons() & data.mask;
    }
}

void QnServerResourceWidget::updateHoverKey() {
    for(QHash<QString, GraphData>::iterator pos = m_graphDataByKey.begin(); pos != m_graphDataByKey.end(); pos++) {
        GraphData &data = *pos;

        if(data.button && data.button->isHovered() && data.button->isChecked()) {
            m_hoveredKey = pos.key();
            return;
        }
    }

    m_hoveredKey = QString();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnServerResourceWidget::tick(int deltaMSecs) {
    qreal delta = 4.0 * deltaMSecs / 1000.0;

    for(QHash<QString, GraphData>::iterator pos = m_graphDataByKey.begin(); pos != m_graphDataByKey.end(); pos++) {
        GraphData &data = *pos;
        if(m_hoveredKey.isEmpty()) {
            data.opacity = qMin(data.opacity + delta, 1.0);
        } else {
            if(pos.key() == m_hoveredKey) {
                data.opacity = qMin(data.opacity + delta, 1.0);
            } else {
                data.opacity = qMax(data.opacity - delta, 0.2);
            }
        }
    }
}

bool QnServerResourceWidget::isLegendVisible() const {
    bool detailsVisible = options().testFlag(DisplayInfo);
    return detailsVisible || isHovered();
}

void QnServerResourceWidget::createButtons()
{
    auto showLogButton = createStatisticAwareButton("server_widget_show_log");
    showLogButton->setIcon(loadSvgIcon("item/log.svg"));
    showLogButton->setCheckable(false);
    showLogButton->setToolTip(tr("Show Log"));
    setHelpTopic(showLogButton, HelpTopic::Id::MainWindow_MonitoringItem_Log);
    connect(showLogButton, &QnImageButtonWidget::clicked, this,
        [this] {menu()->trigger(action::ServerLogsAction, m_resource);});
    titleBar()->rightButtonsBar()->addButton(Qn::ShowLogButton, showLogButton);

    auto checkIssuesButton = createStatisticAwareButton("server_widget_check_issues");
    checkIssuesButton->setIcon(loadSvgIcon("item/checkissues.svg"));
    checkIssuesButton->setCheckable(false);
    checkIssuesButton->setToolTip(tr("Check Issues"));
    connect(checkIssuesButton, &QnImageButtonWidget::clicked, this,
        [this] { menu()->trigger(action::ServerIssuesAction, m_resource); });
    titleBar()->rightButtonsBar()->addButton(Qn::CheckIssuesButton, checkIssuesButton);
}

void QnServerResourceWidget::updateHud(bool animate /*= true*/) {
    base_type::updateHud(animate);
    bool visible = isLegendVisible();

    for (int i = 0; i < ButtonBarCount; i++)
        setOverlayWidgetVisible(m_legendButtonBar[i], visible, animate);
}

QString QnServerResourceWidget::calculateTitleText() const
{
    const auto name = QnResourceDisplayInfo(m_resource).toString(Qn::RI_WithUrl);
    if (m_resource->getStatus() != nx::vms::api::ResourceStatus::online)
        return name;

    const auto uptimeMs = std::chrono::milliseconds(m_manager->uptimeMs(m_resource));
    return tr("%1 (up %2)").arg(name, nx::vms::text::HumanReadable::timeSpan(uptimeMs));
}

int QnServerResourceWidget::calculateButtonsVisibility() const
{
    int result = base_type::calculateButtonsVisibility();
    result &= (Qn::CloseButton | Qn::RotateButton | Qn::InfoButton);
    if (qnRuntime->isDesktopMode())
    {
        if (menu()->canTrigger(action::ServerLogsAction, m_resource))
            result |= Qn::ShowLogButton;

        if (menu()->canTrigger(action::ServerIssuesAction, m_resource))
            result |= Qn::CheckIssuesButton;
    }
    return result;
}

Qn::ResourceStatusOverlay QnServerResourceWidget::calculateStatusOverlay() const
{
    if (qnRuntime->isVideoWallMode() && !isVideoWallLicenseValid())
        return Qn::VideowallWithoutLicenseOverlay;

    auto status = m_resource->getStatus();

    if (status == nx::vms::api::ResourceStatus::offline)
        return Qn::ServerOfflineOverlay;
    if (status == nx::vms::api::ResourceStatus::unauthorized)
       return Qn::ServerUnauthorizedOverlay;
    if (status == nx::vms::api::ResourceStatus::mismatchedCertificate)
        return Qn::MismatchedCertificateOverlay;

    /* Suppress 'No video' overlay */
    return base_type::calculateStatusOverlay(status, true);
}

HealthMonitoringButtons QnServerResourceWidget::savedCheckedHealthMonitoringButtons() const
{
    if (auto workbenchItem = this->item())
    {
        return workbenchItem->data(Qn::ItemHealthMonitoringButtonsRole)
            .value<HealthMonitoringButtons>();
    }
    return {};
}

void QnServerResourceWidget::updateCheckedHealthMonitoringButtons()
{
    setCheckedHealthMonitoringButtons(savedCheckedHealthMonitoringButtons());
}

void QnServerResourceWidget::atItemDataChanged(Qn::ItemDataRole role)
{
    base_type::atItemDataChanged(role);
    if (role != Qn::ItemHealthMonitoringButtonsRole)
        return;
    updateCheckedHealthMonitoringButtons();
}

void QnServerResourceWidget::at_statistics_received() {
    m_updatePeriod = m_manager->updatePeriod(m_resource);

    qint64 id = m_manager->historyId(m_resource);
    if (id < 0) {
        m_renderStatus = Qn::CannotRender;
        return;
    }

    if (id == m_lastHistoryId || m_resource->getStatus() != nx::vms::api::ResourceStatus::online) {
        m_renderStatus = Qn::OldFrameRendered;
        return;
    }

    m_history = m_manager->history(m_resource);
    bool reSort = true;
    if (m_history.keys().size() == m_sortedKeys.size()) {
        foreach (QString key, m_sortedKeys)
            if (!m_history.contains(key))
                break;
        reSort = false;
    }

    if (reSort) {
        m_sortedKeys.clear();

        QList<QnStatisticsData> tmp(m_history.values());
        std::sort(tmp.begin(), tmp.end(), statisticsDataLess);
        foreach(QnStatisticsData key, tmp)
            m_sortedKeys.append(key.description);

        updateLegend();
    }

    m_lastHistoryId = id;
    m_renderStatus = Qn::NewFrameRendered;

    m_elapsedTimer.restart();
    m_counter++;

    updateTitleText();
}
