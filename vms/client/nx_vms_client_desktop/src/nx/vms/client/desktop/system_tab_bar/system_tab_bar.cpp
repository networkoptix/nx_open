// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_tab_bar.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QtEvents>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyleOptionTab>
#include <QtWidgets/QToolButton>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/common/indents.h>
#include <ui/widgets/main_window_title_bar_state.h>
#include <ui/widgets/system_tab_bar_state_handler.h>
#include <ui/workaround/hidpi_workarounds.h>

#include "private/close_tab_button.h"

namespace {

static constexpr int kFixedTabHeight = 32;
static constexpr int kMinimumTabWidth = 64;
static constexpr int kMaximumTabWidth = 196;
static constexpr QnIndents kTabPaddings = {14, 10};

static constexpr int kAllDecorationsWidth =
    kTabPaddings.left()
    + nx::style::Metrics::kInterSpace
    + nx::vms::client::desktop::CloseTabButton::kFixedIconWidth
    + kTabPaddings.right();

} // namespace

namespace nx::vms::client::desktop {

SystemTabBar::SystemTabBar(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    setMovable(false);
    setSelectionBehaviorOnRemove(QTabBar::SelectLeftTab);
    setDrawBase(false);
    setTabsClosable(false);
    setElideMode(Qt::ElideRight);
    setTabShape(this, nx::style::TabShape::Rectangular);
    setUsesScrollButtons(false);
    setFixedHeight(kFixedTabHeight);
    setMaximumWidth(0); //< The initial value. It will be changed when tabs are added.
    setContentsMargins(1, 0, 1, 0);
    setProperty(style::Properties::kSideIndentation, QVariant::fromValue(kTabPaddings));

    // Set palette colors for tabs:
    // - QPalette::Text - text;
    // - QPalette::Base - background;
    // - QPalette::Dark - right margin;
    // - QPalette::Light - left margin.
    auto p = palette();

    // Active tab:
    p.setColor(QPalette::Active, QPalette::Text, core::colorTheme()->color("light4"));
    p.setColor(QPalette::Active, QPalette::Base, core::colorTheme()->color("dark10"));
    p.setColor(QPalette::Active, QPalette::Light, core::colorTheme()->color("dark4"));
    p.setColor(QPalette::Active, QPalette::Dark, core::colorTheme()->color("dark4"));

    // Inactive tab:
    p.setColor(QPalette::Inactive, QPalette::Text, core::colorTheme()->color("light16"));
    p.setColor(QPalette::Inactive, QPalette::Base, core::colorTheme()->color("dark7"));
    p.setColor(QPalette::Inactive, QPalette::Light, core::colorTheme()->color("dark6"));
    p.setColor(QPalette::Inactive, QPalette::Dark, core::colorTheme()->color("dark8"));
    setPalette(p);

    connect(this, &QTabBar::currentChanged, this, &SystemTabBar::at_tabActivated);
    connect(this, &QTabBar::tabBarClicked, this, &SystemTabBar::at_tabActivated);
    connect(this, &QTabBar::tabCloseRequested, this, &SystemTabBar::at_tabCloseRequested);
    connect(this, &QTabBar::tabMoved, this, &SystemTabBar::at_tabMoved);
}

void SystemTabBar::setStateStore(QSharedPointer<Store> store)
{
    if (m_store)
        m_store->disconnect(this);

    m_store = store;

    if (m_store)
        connect(m_store.get(), &Store::stateChanged, this, &SystemTabBar::at_storeStateChanged);
}

void SystemTabBar::rebuildTabs(const State& state)
{
    m_tabWidthCalculator.clear();

    for (int i = 0; i < state.sessions.size(); ++i)
    {
        const QString& text = state.sessions[i].systemName;

        if (i < count())
        {
            setTabText(i, text);
        }
        else
        {
            addTab(text);
            CloseTabButton::createForTab(this, i);
        }
        setTabToolTip(i, text);

        m_tabWidthCalculator.addTab(calculateTabWidth(text));
    }

    while (count() > state.sessions.size())
        removeTab(count() - 1);

    setMovable(count() > 1);
    setMaximumWidth(m_tabWidthCalculator.originalFullWidth());
}

bool SystemTabBar::active() const
{
    if (!m_store)
        return true;

    const State& state = m_store->state();
    return !state.homeTabActive && state.activeSessionId;
}

QSize SystemTabBar::tabSizeHint(int index) const
{
    int width = index < m_tabWidthCalculator.count()
        ? m_tabWidthCalculator.width(index)
        : base_type::tabSizeHint(index).width();
    if (width > kMaximumTabWidth)
        width = kMaximumTabWidth;
    return QSize(width, kFixedTabHeight);
}

QSize SystemTabBar::minimumTabSizeHint(int /*index*/) const
{
    return QSize(kMinimumTabWidth, kFixedTabHeight);
}

void SystemTabBar::contextMenuEvent(QContextMenuEvent* event)
{
    if (!NX_ASSERT(m_store))
        return;

    const State& state = m_store->state();

    const int index = tabAt(event->pos());
    if (!NX_ASSERT(index < state.sessions.size()))
        return;

    QMenu menu(this);
    menu.addAction(tr("Close"),
        [this, index]() { emit tabCloseRequested(index); });
    menu.addAction(tr("Open in New Window"),
        [this, sessionId = state.sessions[index].sessionId]()
        {
            // This action does cannot change the tab bar state, so it can be invoked directly.
            this->menu()->trigger(menu::OpenSessionInNewWindowAction,
                menu::Parameters().withArgument(Qn::SessionIdRole, sessionId));
        });

    QnHiDpiWorkarounds::showMenu(&menu, QCursor::pos());
}

bool SystemTabBar::areTabsChanged(const State& state) const
{
    if (state.sessions.size() != count())
        return true;

    for (int i = 0; i < state.sessions.size(); ++i)
    {
        if (state.sessions[i].systemName != tabText(i))
            return true;
    }

    return false;
}

void SystemTabBar::at_storeStateChanged(const State& state)
{
    if (areTabsChanged(state))
        rebuildTabs(state);

    setCurrentIndex((!state.homeTabActive && state.activeSessionId)
        ? state.sessionIndex(*state.activeSessionId)
        : -1);
}

void SystemTabBar::initStyleOption(QStyleOptionTab* option, int tabIndex) const
{
    base_type::initStyleOption(option, tabIndex);
    if (m_store && m_store->state().homeTabActive)
        option->state &= ~QStyle::State_Selected;
    if (tabIndex < m_tabWidthCalculator.count())
    {
        int x = 0;
        for (int i = 0; i < tabIndex; ++i)
            x += m_tabWidthCalculator.width(i);

        option->rect = QRect(QPoint(x, 0), QSize(m_tabWidthCalculator.width(tabIndex), kFixedTabHeight));
        option->text = fontMetrics().elidedText(
            option->text,
            elideMode(),
            m_tabWidthCalculator.width(tabIndex) - kAllDecorationsWidth,
            Qt::TextShowMnemonic);
    }
}

void SystemTabBar::resizeEvent(QResizeEvent* event)
{
    QScopedValueRollback<bool> guard(m_resizing, true);
    m_tabWidthCalculator.calculate(event->size().width());
    base_type::resizeEvent(event);
}

void SystemTabBar::tabLayoutChange()
{
    if (m_resizing)
        return;

    m_tabWidthCalculator.reset();
    adjustSize();
}

void SystemTabBar::wheelEvent(QWheelEvent* event)
{
    const bool wheelVertical = qAbs(event->angleDelta().y()) > qAbs(event->angleDelta().x());
    if (wheelVertical
        && event->device()->capabilities().testFlag(QInputDevice::Capability::PixelScroll))
    {
        const int delta = event->pixelDelta().y();
        if (delta != 0)
        {
            if (currentIndex() >= 0)
            {
                const int newIndex = (currentIndex() + (delta > 0 ? 1 : -1) + count()) % count();
                setCurrentIndex(newIndex);
            }
        }
        event->accept();
    }
    else
    {
        base_type::wheelEvent(event);
    }
}

int SystemTabBar::calculateTabWidth(const QString& text) const
{
    // The width of the tab is the sum of tab text width, width of the close button,
    // space between them and left and right paddings.
    return kAllDecorationsWidth + fontMetrics().size(Qt::TextShowMnemonic, text).width();
}

void SystemTabBar::at_tabActivated(int index)
{
    if (!NX_ASSERT(m_store))
        return;

    m_store->setActiveTab(index);
}

void SystemTabBar::at_tabCloseRequested(int index)
{
    if (!NX_ASSERT(m_store))
        return;

    const State& state = m_store->state();

    if (!NX_ASSERT(index < state.sessions.size()))
        return;

    m_store->disconnectAndRemoveSession(state.sessions[index].sessionId,
        [this]()
        {
            return windowContext()->connectActionsHandler()->askDisconnectConfirmation();
        });
}

void SystemTabBar::at_tabMoved(int from, int to)
{
    if (!NX_ASSERT(m_store))
        return;

    m_tabWidthCalculator.moveTab(from, to);
    m_store->moveSession(from, to);
}

} // namespace nx::vms::client::desktop
