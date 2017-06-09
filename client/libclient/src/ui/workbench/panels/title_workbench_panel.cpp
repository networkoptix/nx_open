#include "title_workbench_panel.h"

#include <nx/client/ui/workbench/workbench_animations.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>

#include <ui/actions/action_manager.h>
#include <ui/animation/animator_group.h>
#include <ui/animation/opacity_animator.h>
#include <ui/animation/variant_animator.h>
#include <ui/graphics/instruments/hand_scroll_instrument.h>
#include <ui/graphics/instruments/motion_selection_instrument.h>
#include <ui/graphics/items/generic/edge_shadow_widget.h>
#include <ui/graphics/items/generic/image_button_widget.h>
#include <ui/graphics/items/generic/masked_proxy_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/widgets/main_window_title_bar_widget.h>

#include <ui/workbench/workbench_ui_globals.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_pane_settings.h>
#include <ui/workbench/panels/buttons.h>

#include <utils/common/scoped_value_rollback.h>

namespace NxUi {

TitleWorkbenchPanel::TitleWorkbenchPanel(
    const QnPaneSettings& settings,
    QGraphicsWidget* parentWidget,
    QObject* parent)
    :
    base_type(settings, parentWidget, parent),
    item(new QnMaskedProxyWidget(parentWidget)),
    m_showButton(NxUi::newShowHideButton(parentWidget, context(),
        action(QnActions::ToggleTitleBarAction))),
    m_opacityAnimatorGroup(new AnimatorGroup(this)),
    m_yAnimator(new VariantAnimator(this)),
    m_opacityProcessor(new HoverFocusProcessor(parentWidget))
{
    item->setProperty(Qn::NoHandScrollOver, true);
    item->setProperty(Qn::BlockMotionSelection, true);
    item->setWidget(new QnMainWindowTitleBarWidget(nullptr, context()));
    item->setPos(0.0, 0.0);
    item->setVisible(false);
    item->setZValue(ControlItemZOrder);
    connect(item, &QGraphicsWidget::geometryChanged, this,
        &TitleWorkbenchPanel::updateControlsGeometry);

    const auto toggleTitleBarAction = action(QnActions::ToggleTitleBarAction);
    toggleTitleBarAction->setChecked(settings.state == Qn::PaneState::Opened);
    {
        QTransform transform;
        transform.rotate(-90);
        transform.scale(-1, 1);
        m_showButton->setTransform(transform);
    }
    m_showButton->setFocusProxy(item);

    connect(toggleTitleBarAction, &QAction::toggled, this,
        [this](bool checked)
        {
            if (!m_ignoreClickEvent)
                setOpened(checked, true);
        });

    m_opacityProcessor->addTargetItem(item);
    m_opacityProcessor->addTargetItem(m_showButton);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverEntered, this,
        &AbstractWorkbenchPanel::hoverEntered);
    connect(m_opacityProcessor, &HoverFocusProcessor::hoverLeft, this,
        &AbstractWorkbenchPanel::hoverLeft);

    m_yAnimator->setTimer(animationTimer());
    m_yAnimator->setTargetObject(item);
    m_yAnimator->setAccessor(new PropertyAccessor("y"));

    m_opacityAnimatorGroup->setTimer(animationTimer());
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(item));
    m_opacityAnimatorGroup->addAnimator(opacityAnimator(m_showButton));

    /* Create a shadow: */
    auto shadow = new QnEdgeShadowWidget(parentWidget, item, Qt::BottomEdge, NxUi::kShadowThickness);
    shadow->setZValue(NxUi::ShadowItemZOrder);

    updateControlsGeometry();
}

bool TitleWorkbenchPanel::isPinned() const
{
    return true;
}

bool TitleWorkbenchPanel::isOpened() const
{
    return action(QnActions::ToggleTitleBarAction)->isChecked();
}

void TitleWorkbenchPanel::setOpened(bool opened, bool animate)
{
    using namespace nx::client::ui::workbench;

    ensureAnimationAllowed(&animate);

    QN_SCOPED_VALUE_ROLLBACK(&m_ignoreClickEvent, true);
    action(QnActions::ToggleTitleBarAction)->setChecked(opened);

    m_yAnimator->stop();
    qnWorkbenchAnimations->setupAnimator(m_yAnimator, opened
        ? Animations::Id::TitlePanelExpand
        : Animations::Id::TitlePanelCollapse);

    qreal newY = opened
        ? 0.0
        : -item->size().height() - kHidePanelOffset;

    if (animate)
        m_yAnimator->animateTo(newY);
    else
        item->setY(newY);

    emit openedChanged(opened, animate);
}

bool TitleWorkbenchPanel::isVisible() const
{
    return m_visible && m_used;
}

void TitleWorkbenchPanel::setVisible(bool visible, bool animate)
{
    if (visible && !isUsed())
        return;

    ensureAnimationAllowed(&animate);

    bool changed = m_visible != visible;

    m_visible = visible;

    updateOpacity(animate);
    if (changed)
        emit visibleChanged(visible, animate);
}

qreal TitleWorkbenchPanel::opacity() const
{
    return opacityAnimator(item)->targetValue().toDouble();
}

void TitleWorkbenchPanel::setOpacity(qreal opacity, bool animate)
{
    ensureAnimationAllowed(&animate);

    if (animate)
    {
        m_opacityAnimatorGroup->pause();
        opacityAnimator(item)->setTargetValue(opacity);
        opacityAnimator(m_showButton)->setTargetValue(opacity);
        m_opacityAnimatorGroup->start();
    }
    else
    {
        m_opacityAnimatorGroup->stop();
        item->setOpacity(opacity);
        m_showButton->setOpacity(opacity);
    }
}

bool TitleWorkbenchPanel::isHovered() const
{
    return m_opacityProcessor->isHovered();
}

void TitleWorkbenchPanel::stopAnimations()
{
    if (!m_yAnimator->isRunning())
        return;

    m_yAnimator->stop();
    item->setY(m_yAnimator->targetValue().toDouble());
}

bool TitleWorkbenchPanel::isUsed() const
{
    return m_used;
}

QRectF TitleWorkbenchPanel::effectiveGeometry() const
{
    QRectF geometry = item->geometry();
    if (m_yAnimator->isRunning())
        geometry.moveTop(m_yAnimator->targetValue().toReal());
    return geometry;
}

void TitleWorkbenchPanel::updateControlsGeometry()
{
    auto parentWidgetRect = m_parentWidget->rect();
    QRectF geometry = item->geometry();

    m_showButton->setPos(QPointF(
        (geometry.left() + geometry.right() - m_showButton->size().height()) / 2,
        qMax(parentWidgetRect.top(), geometry.bottom())));

    emit geometryChanged();
}

void TitleWorkbenchPanel::setUsed(bool value)
{
    if (m_used == value)
        return;

    m_used = value;
    setVisible(value, false); //< visibility via opacity
    item->setVisible(value); //< true visibility on the scene
}

} //namespace NxUi
