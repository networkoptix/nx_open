// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "jump_to_live_button_p.h"

#include <cmath>

#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsDropShadowEffect>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/icon.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/animation/opacity_animator.h>
#include <ui/graphics/items/generic/slider_tooltip_widget.h>
#include <ui/processors/hover_processor.h>
#include <ui/workaround/sharp_pixmap_painting.h>
#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>

namespace {

/* Opacity for disabled button: */
static const qreal kDisabledTriggerOpacity = nx::style::Hints::kDisabledItemOpacity;

/* Tooltip adjustments: */
static constexpr int kHoverEnterDelayMs = 0;
static constexpr int kHoverLeaveDelayMs = 25;
static constexpr qreal kToolTipFadeTimeMs = 25;
static constexpr qreal kToolTipRoundingRadius = 2.0;
static constexpr qreal kToolTipPadding = 8.0;
static constexpr qreal kToolTipShadowBlurRadius = 5.0;
static constexpr qreal kToolTipShadowOpacity = 0.36;
static const QPointF kToolTipShadowOffset(0.0, 3.0);

static Qt::Edge invertEdge(Qt::Edge edge)
{
    switch (edge)
    {
        case Qt::LeftEdge:
            return Qt::RightEdge;

        case Qt::RightEdge:
            return Qt::LeftEdge;

        case Qt::TopEdge:
            return Qt::BottomEdge;

        case Qt::BottomEdge:
            return Qt::TopEdge;

        default:
            NX_ASSERT(false);
            return Qt::LeftEdge;
    }
}

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kLight1Theme = {
    {QnIcon::Normal, {.primary = "light1"}},
};

NX_DECLARE_COLORIZED_ICON(kGoToLiveIcon, "24x24/Solid/go_to_live.svg", kLight1Theme)

} // namespace

namespace nx::vms::client::desktop {

using State = CameraButton::State;

JumpToLiveButtonPrivate::JumpToLiveButtonPrivate(JumpToLiveButton* main):
    base_type(nullptr),
    q_ptr(main),
    m_toolTip(new QnSliderTooltipWidget(main)),
    m_toolTipHoverProcessor(new HoverFocusProcessor(main))
{
    Q_Q(JumpToLiveButton);

    connect(main, &JumpToLiveButton::geometryChanged,
        this, &JumpToLiveButtonPrivate::updateToolTipPosition);

    connect(main, &JumpToLiveButton::pressed, this,
        [q]() { q->setState(State::Default); });

    installEventHandler(main, QEvent::PaletteChange, this,
        [this, q]()
        {
            m_imagesDirty = true;
            q->update();
        });

    m_toolTipHoverProcessor->addTargetItem(main);
    m_toolTipHoverProcessor->setHoverEnterDelay(kHoverEnterDelayMs);
    m_toolTipHoverProcessor->setHoverLeaveDelay(kHoverLeaveDelayMs);

    connect(m_toolTipHoverProcessor, &HoverFocusProcessor::hoverEntered,
        this, &JumpToLiveButtonPrivate::updateToolTipVisibility);
    connect(m_toolTipHoverProcessor, &HoverFocusProcessor::hoverLeft,
        this, &JumpToLiveButtonPrivate::updateToolTipVisibility);

    m_toolTip->setOpacity(0.0);
    m_toolTip->setRoundingRadius(kToolTipRoundingRadius);
    m_toolTip->setFlag(QGraphicsItem::ItemIgnoresTransformations, false);

    auto shadowEffect = new QGraphicsDropShadowEffect(m_toolTip);
    auto shadowColor = QPalette().color(QPalette::Shadow);
    shadowColor.setAlphaF(kToolTipShadowOpacity);
    shadowEffect->setColor(shadowColor);
    shadowEffect->setOffset(kToolTipShadowOffset);
    shadowEffect->setBlurRadius(kToolTipShadowBlurRadius);
    m_toolTip->setGraphicsEffect(shadowEffect);

    updateToolTipTailEdge();

    connect(q, &CameraButton::iconChanged, this, [this]() { m_imagesDirty = true; });

    const auto updateTooltipVisibilityConnections =
        [this, q]()
        {
            m_imagesDirty = true;
            if (q->prolonged())
            {
                connect(q, &JumpToLiveButton::pressed,
                    this, &JumpToLiveButtonPrivate::updateToolTipVisibility);
                connect(q, &JumpToLiveButton::released,
                    this, &JumpToLiveButtonPrivate::updateToolTipVisibility);
            }
            else
            {
                disconnect(q, &JumpToLiveButton::pressed,
                    this, &JumpToLiveButtonPrivate::updateToolTipVisibility);
                disconnect(q, &JumpToLiveButton::released,
                    this, &JumpToLiveButtonPrivate::updateToolTipVisibility);
            }
            if (q->scene())
                updateToolTipVisibility();
        };

    connect(q, &CameraButton::prolongedChanged, this, updateTooltipVisibilityConnections);
    updateTooltipVisibilityConnections();

    connect(q, &CameraButton::stateChanged, this, [this]() { updateState(); });
    connect(q, &CameraButton::isLiveChanged, this,
        [this, q]()
        {
            updateToolTipText();
            updateToolTipVisibility();
            q->update();
        });
}

JumpToLiveButtonPrivate::~JumpToLiveButtonPrivate()
{
}

QString JumpToLiveButtonPrivate::toolTip() const
{
    return m_toolTip->text();
}

void JumpToLiveButtonPrivate::setToolTip(const QString& toolTip)
{
    if (m_toolTipText == toolTip)
        return;

    m_toolTipText = toolTip;
    updateToolTipText();
}

void JumpToLiveButtonPrivate::updateToolTipText()
{
    Q_Q(JumpToLiveButton);
    m_toolTip->setText(q->isLive()
        ? m_toolTipText
        : JumpToLiveButton::tr("Go to Live"));

    updateToolTipPosition();
    if (m_toolTip->opacity() != 0.0)
        updateToolTipVisibility();
}

Qt::Edge JumpToLiveButtonPrivate::toolTipEdge() const
{
    return m_toolTipEdge;
}

void JumpToLiveButtonPrivate::setToolTipEdge(Qt::Edge edge)
{
    if (m_toolTipEdge == edge)
        return;

    m_toolTipEdge = edge;
    updateToolTipTailEdge();
}

void JumpToLiveButtonPrivate::updateToolTipVisibility()
{
    static constexpr qreal kTransparent = 0.0;
    static constexpr qreal kOpaque = 1.0;

    Q_Q(const JumpToLiveButton);

    const bool showToolTip = m_toolTipHoverProcessor->isHovered()
        && !m_toolTip->text().isEmpty()
        && !(q->prolonged() && q->isPressed());

    static constexpr qreal kToolTipAnimationSpeedFactor = 1000.0 / kToolTipFadeTimeMs;

    const qreal targetOpacity = showToolTip ? kOpaque : kTransparent;
    opacityAnimator(m_toolTip, kToolTipAnimationSpeedFactor)->animateTo(targetOpacity);
}

void JumpToLiveButtonPrivate::updateToolTipTailEdge()
{
    m_toolTip->setTailEdge(invertEdge(m_toolTipEdge));
    updateToolTipPosition();
}

void JumpToLiveButtonPrivate::updateToolTipPosition()
{
    Q_Q(const JumpToLiveButton);
    const auto size = q->size();

    switch (m_toolTipEdge)
    {
        case Qt::LeftEdge:
            m_toolTip->pointTo({ -kToolTipPadding, size.height() / 2 });
            break;

        case Qt::RightEdge:
            m_toolTip->pointTo({ size.width() + kToolTipPadding - 1, size.height() / 2 });
            break;

        case Qt::TopEdge:
            m_toolTip->pointTo({ size.width() / 2, -kToolTipPadding });
            break;

        case Qt::BottomEdge:
            m_toolTip->pointTo({ size.width() / 2, size.height() + kToolTipPadding - 1 });
            break;

        default:
            NX_ASSERT(false);
    }
}

void JumpToLiveButtonPrivate::updateState()
{
    if (m_updatingState)
        return;

    const QScopedValueRollback updatingStateRollback(m_updatingState, true);

    Q_Q(JumpToLiveButton);
    q->update();
}

void JumpToLiveButtonPrivate::paint(QPainter* painter,
    const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_Q(JumpToLiveButton);

    if (!q->isLive() && (q->isHovered() || q->isPressed()))
    {
        ensureImages();

        paintPixmapSharp(painter, q->isPressed()
            ? m_goToLivePixmapPressed
            : m_goToLivePixmap);
    }
    else
    {
        if (!q->isLive())
            painter->setOpacity(painter->opacity() * kDisabledTriggerOpacity);

        q->base_type::paint(painter, option, widget);
    }
}

void JumpToLiveButtonPrivate::ensureImages()
{
    if (!m_imagesDirty)
        return;

    m_imagesDirty = false;

    Q_Q(JumpToLiveButton);

    const auto goToLivePixmap = qnSkin->icon(kGoToLiveIcon).pixmap(24, 24);

    m_goToLivePixmap = q->generatePixmap(
        q->activeColor(),
        QColor(),
        goToLivePixmap);

    m_goToLivePixmapPressed = q->generatePixmap(
        q->pressedColor(),
        QColor(),
        goToLivePixmap);
}

} // namespace nx::vms::client::desktop
