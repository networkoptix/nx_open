// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "compact_tab_bar.h"

#include <chrono>

#include <QtCore/QVariantAnimation>
#include <QtGui/QHoverEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolTip>

#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <ui/common/palette.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

using namespace std::chrono;

static constexpr int kTabHeight = 32;
static constexpr int kTabMargin = 8; //< Tab horizontal margin.
static constexpr int kTabInnerSpacing = 4; //< Spacing between tab icon and text.
static constexpr milliseconds kAnimationDuration = 200ms;

} // namespace

//-------------------------------------------------------------------------------------------------
// CompactTabBar::Private interface

class CompactTabBar::Private
{
    CompactTabBar* const q = nullptr;

public:
    Private(CompactTabBar* q);
    ~Private() = default;

    void paint();
    QSize tabSizeHint(int index) const;

    int hoveredTab() const;
    void setHoveredTab(int index);

    void tabInserted(int index);
    void tabRemoved(int index);
    void tabMoved(int from, int to);

    void handleCurrentChanged();

    void setCustomTabEnabledFunction(const CustomTabEnabledFunction& tabEnabledFunction);
private:
    void paintTab(int index, QPainter* painter);
    void updateAnimation(int index);
    void updateLayout();
    bool tabCountChanging() const; //< Whether a tab is being inserted or removed.
    QIcon tabIcon(int index) const; //< Ensures mandatory icon by placeholder if needed.

private:
    int m_hoveredTab = -1;
    QList<QVariantAnimation*> m_animations; //< Animations are owned by q.
    CustomTabEnabledFunction m_customTabEnabledFunction;
};

//-------------------------------------------------------------------------------------------------
// CompactTabBar::Private implementation

CompactTabBar::Private::Private(CompactTabBar* q) : q(q)
{
}

void CompactTabBar::Private::paint()
{
    if (tabCountChanging())
    {
        // Queue repaint for later.
        q->update();
        return;
    }

    QPainter painter(q);

    for (int index = 0; index < q->count(); ++index)
        paintTab(index, &painter);
}

QSize CompactTabBar::Private::tabSizeHint(int index) const
{
    const auto iconSize = qnSkin->maximumSize(tabIcon(index));

    const auto tabWithIconWidth = iconSize.width() + kTabMargin * 2;

    const auto iconsOnlyWidth = q->count() * tabWithIconWidth;

    const auto textSize = QFontMetrics(q->font()).size(0, q->tabText(index));

    const auto fraction = tabCountChanging() ? 0.0 : m_animations[index]->currentValue().toReal();

    // Reduce rounding errors to prevent widths jitter while switching tabs.
    const int jitterFix = fraction > 0 && fraction < 1.0 ? 1 : 0;

    const auto textWidth = q->parentWidget()->width() - iconsOnlyWidth + jitterFix;
    const auto visibleTextWidth = static_cast<int>(textWidth * fraction);

    const QSize result(
        iconSize.width() + visibleTextWidth + kTabMargin * 2,
        std::max({textSize.height(), iconSize.height(), q->minimumHeight(), kTabHeight}));

    return result;
}

int CompactTabBar::Private::hoveredTab() const
{
    return m_hoveredTab;
}

void CompactTabBar::Private::setHoveredTab(int index)
{
    if (m_hoveredTab == index)
        return;

    m_hoveredTab = index;
    q->update();

    emit q->tabHoverChanged(index);
}

void CompactTabBar::Private::tabInserted(int index)
{
    auto animation = new QVariantAnimation(q);
    animation->setEasingCurve(QEasingCurve::InOutCubic);
    animation->setDuration(kAnimationDuration.count());
    animation->setStartValue(0.0);
    animation->setEndValue(0.0);

    q->connect(animation, &QVariantAnimation::valueChanged,
        [this]() { updateLayout(); });

    m_animations.insert(index, animation);
    handleCurrentChanged();
    updateLayout();
}

void CompactTabBar::Private::tabRemoved(int index)
{
    auto animation = m_animations.takeAt(index);
    animation->stop();
    animation->deleteLater();
    handleCurrentChanged();
    updateLayout();
}

void CompactTabBar::Private::tabMoved(int from, int to)
{
    m_animations.insert(to, m_animations.takeAt(from));
    updateLayout();
}

void CompactTabBar::Private::handleCurrentChanged()
{
    if (tabCountChanging())
        return;

    for (int index = 0; index < q->count(); ++index)
        updateAnimation(index);
}

void CompactTabBar::Private::setCustomTabEnabledFunction(
    const CustomTabEnabledFunction& tabEnabledFunction)
{
    m_customTabEnabledFunction = tabEnabledFunction;
}

void CompactTabBar::Private::paintTab(int index, QPainter* painter)
{
    // Get state.
    const auto current = q->currentIndex() == index;
    bool enabled = q->isTabEnabled(index);
    if (m_customTabEnabledFunction)
    {
        enabled = m_customTabEnabledFunction(index);
        if (!enabled && current)
        {
            // If the tab is current it must be drawn as enabled, even if custom state is disabled.
            enabled = true;
        }
    }
    const auto hovered = enabled && m_hoveredTab == index;

    // Get data.
    const auto rect = q->tabRect(index).adjusted(kTabMargin, 0, -kTabMargin, 0);
    const auto text = q->tabText(index);
    const auto icon = tabIcon(index);
    NX_ASSERT(!icon.isNull());

    // Draw icon.

    const auto textSize = QFontMetrics(q->font()).size(0, text);

    const auto iconSize = qnSkin->maximumSize(icon);

    const auto fraction = tabCountChanging()
        ? 0.0
        : m_animations[index]->currentValue().toReal();

    const auto textWidth = static_cast<int>(textSize.width() * fraction);

    QSize textAndIconSize(
        iconSize.width() + (current ? (kTabInnerSpacing + textWidth) : 0),
        std::max(iconSize.height(), textSize.height()));

    const auto textAndIconRect = QStyle::alignedRect(q->layoutDirection(),
        Qt::AlignHCenter | Qt::AlignVCenter, textAndIconSize, rect);

    const auto iconRect = QStyle::alignedRect(q->layoutDirection(),
        Qt::AlignLeft | Qt::AlignVCenter, iconSize, textAndIconRect);

    icon.paint(painter, iconRect, Qt::AlignCenter,
        [enabled, current, hovered]()
        {
            if (!enabled)
                return QIcon::Disabled;
            if (current)
                return QIcon::Active;
            if (hovered)
                return QIcon::Selected;

            return QIcon::Normal;
        }());

    // Draw text.

    auto textRect = rect;
    if (q->layoutDirection() == Qt::RightToLeft)
        textRect.setRight(iconRect.left() - (kTabInnerSpacing + 1));
    else
        textRect.setLeft(iconRect.right() + kTabInnerSpacing + 1);
    textRect.setWidth(textWidth);

    const auto colorGroup = enabled ? QPalette::Normal : QPalette::Disabled;

    painter->setPen(q->palette().color(colorGroup,
        [current, hovered]()
        {
            if (current)
                return QPalette::Highlight;
            if (hovered)
                return QPalette::Light;

            return QPalette::WindowText;
        }()));

    if (textRect.isValid() && current)
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);

    // Draw side border.

    painter->setPen(colorTheme()->color("dark6"));

    const QRect tabRect = q->tabRect(index);

    painter->drawLine(tabRect.topLeft(), tabRect.bottomLeft());

    // Draw underline.

    if (current)
    {
        painter->setPen(colorTheme()->color("brand_core"));
        // Fine-tuned in order to nicely blend the line into widget border.
        painter->translate(0, 0.5);
        painter->drawLine(tabRect.bottomLeft() + QPoint{1, 0}, tabRect.bottomRight());
    }
}

void CompactTabBar::Private::updateAnimation(int index)
{
    const qreal target = index == q->currentIndex() ? 1.0 : 0.0;

    auto animation = m_animations[index];
    if (qFuzzyIsNull(animation->endValue().toReal() - target))
        return;

    const auto start = animation->currentValue().toReal();
    animation->stop();
    animation->setStartValue(start);
    animation->setEndValue(target);
    animation->setDuration(kAnimationDuration.count() * qAbs(target - start));
    animation->start();
}

void CompactTabBar::Private::updateLayout()
{
    // Force tab relayout. We don't have access to QTabBarPrivate to do it directly.
    QEvent dummy(QEvent::FontChange);
    q->changeEvent(&dummy);
    q->resize(q->sizeHint());
}

bool CompactTabBar::Private::tabCountChanging() const
{
    return m_animations.size() != q->count();
}

QIcon CompactTabBar::Private::tabIcon(int index) const
{
    auto result = q->tabIcon(index);
    if (result.isNull())
    {
        // Use placeholder icon.
        static const QnIcon::Suffixes suffixes({{QIcon::Selected, QString()}});
        result = qnSkin->icon(lit("events/alert_red.png"), QString(), &suffixes);
    }

    return result;
}

//-------------------------------------------------------------------------------------------------
// CompactTabBar implementation

CompactTabBar::CompactTabBar(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
    setAttribute(Qt::WA_Hover);

    setExpanding(false);
    setUsesScrollButtons(false);
    setElideMode(Qt::ElideNone);
    setMinimumHeight(style::Metrics::kHeaderSize);

    setPaletteColor(this, QPalette::WindowText, colorTheme()->color("light12"));
    setPaletteColor(this, QPalette::Light, colorTheme()->color("light10"));
    setPaletteColor(this, QPalette::Midlight, colorTheme()->color("dark12"));
    setPaletteColor(this, QPalette::Highlight, colorTheme()->color("brand_core"));

    setPaletteColor(this, QPalette::Disabled, QPalette::WindowText, colorTheme()->color("dark12"));
    setPaletteColor(this, QPalette::Disabled, QPalette::Light, colorTheme()->color("dark12"));
    setPaletteColor(this, QPalette::Disabled, QPalette::Midlight, colorTheme()->color("dark12"));
    setPaletteColor(this, QPalette::Disabled, QPalette::Highlight, colorTheme()->color("dark12"));

    connect(this, &QTabBar::tabMoved, [this](int from, int to) { d->tabMoved(from, to); });
    connect(this, &QTabBar::currentChanged, [this]() { d->handleCurrentChanged(); });
}

QSize CompactTabBar::tabSizeHint(int index) const
{
    return d->tabSizeHint(index);
}

QSize CompactTabBar::minimumTabSizeHint(int index) const
{
    return d->tabSizeHint(index);
}

void CompactTabBar::paintEvent(QPaintEvent* /*event*/)
{
    d->paint();
}

bool CompactTabBar::event(QEvent* event)
{
    switch (event->type())
    {
        case QEvent::HoverEnter:
        case QEvent::HoverMove:
            d->setHoveredTab(tabAt(static_cast<QHoverEvent*>(event)->pos()));
            break;

        case QEvent::HoverLeave:
        case QEvent::Leave:
            d->setHoveredTab(-1);
            break;

        case QEvent::ToolTip:
            if (d->hoveredTab() == currentIndex())
            {
                QToolTip::hideText();
                event->accept();
                return true; //< Do not show tooltip for current tab.
            }
            break;

        default:
            break;
    }

    return base_type::event(event);
}

void CompactTabBar::setCustomTabEnabledFunction(
    const CustomTabEnabledFunction& customTabEnabledFunction)
{
    d->setCustomTabEnabledFunction(customTabEnabledFunction);
}

void CompactTabBar::tabInserted(int index)
{
    d->tabInserted(index);
}

void CompactTabBar::tabRemoved(int index)
{
    d->tabRemoved(index);
}

} // namespace nx::vms::client::desktop
