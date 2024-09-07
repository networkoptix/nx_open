// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_tab_bar.h"

#include <QtCore/QMimeData>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>
#include <QtGui/QDrag>
#include <QtGui/QPainter>
#include <QtGui/QtEvents>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyleOptionTab>
#include <QtWidgets/QToolButton>

#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/data/logon_data.h>
#include <nx/vms/client/desktop/system_logon/ui/welcome_screen.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/common/indents.h>
#include <ui/widgets/main_window.h>
#include <ui/widgets/main_window_title_bar_state.h>
#include <ui/widgets/system_tab_bar_state_handler.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <utils/common/delayed.h>

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

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kSystemSubstitutions =
    {{QnIcon::Normal, {.primary = "light4"}}};
static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kAddSubstitutions =
    {{QnIcon::Normal, {.primary = "dark14"}}};

NX_DECLARE_COLORIZED_ICON(kSystemIcon, "20x20/Solid/system_local.svg", kSystemSubstitutions)
NX_DECLARE_COLORIZED_ICON(kAddIcon, "20x20/Outline/add.svg", kAddSubstitutions)

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

    connect(this, &QTabBar::currentChanged, this, &SystemTabBar::at_currentChanged);
    connect(this, &QTabBar::tabMoved, this, &SystemTabBar::at_tabMoved);
}

void SystemTabBar::setStateStore(QSharedPointer<Store> store,
    QSharedPointer<StateHandler> stateHandler)
{
    m_store = store;

    if (m_stateHandler)
        m_stateHandler->disconnect(this);
    m_stateHandler = stateHandler;
    if (m_stateHandler)
    {
        connect(m_stateHandler.get(), &StateHandler::tabsChanged, this, &SystemTabBar::rebuildTabs);
        connect(m_stateHandler.get(), &StateHandler::activeSystemTabChanged, this,
            [this](int index)
            {
                setCurrentIndex(index);
            });
        connect(m_stateHandler.get(), &StateHandler::homeTabActiveChanged, this,
            [this](bool active)
            {
                if (!NX_ASSERT(m_store))
                    return;

                setCurrentIndex(active ? -1 : m_store->activeSystemTab());
            });
    }
}

void SystemTabBar::rebuildTabs()
{
    if (!NX_ASSERT(m_store) || m_updating)
        return;

    const auto fm = fontMetrics();
    {
        QSignalBlocker blocker(this);
        m_tabWidthCalculator.clear();
        for (int i = 0; i < m_store->systemCount(); ++i)
        {
            const auto systemData = m_store->systemData(i);

            // The width of the tab is the sum of tab text width, width of the close button,
            // space between them and left and right paddings.
            const int tabWidth = kAllDecorationsWidth
                + fm.size(Qt::TextShowMnemonic, systemData->systemDescription->name()).width();
            m_tabWidthCalculator.addTab(tabWidth);

            if (tabData(i).value<core::SystemDescriptionPtr>() == systemData->systemDescription)
                continue;

            removeTab(i);
            insertClosableTab(i, systemData->systemDescription);
        }

        while (count() > m_store->systemCount())
            removeTab(count() - 1);
    }
    setMovable(count() > 1);
}

bool SystemTabBar::isUpdating() const
{
    return m_updating;
}

bool SystemTabBar::active() const
{
    return !m_store
        || (!m_store->isHomeTabActive()
            && m_store->activeSystemTab() >= 0
            && m_store->activeSystemTab() < count());
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

void SystemTabBar::mousePressEvent(QMouseEvent* event)
{
    if (!NX_ASSERT(m_store) || event->button() != Qt::LeftButton)
        return;

    const int index = tabAt(event->pos());
    const auto systemData = m_store->systemData(index);
    if (systemData->systemDescription->localId() == m_store->currentSystemId())
        action(menu::ResourcesModeAction)->setChecked(true);
    if (m_store->activeSystemTab() < 0)
        at_currentChanged(index);
    else
        base_type::mousePressEvent(event);
}

void SystemTabBar::contextMenuEvent(QContextMenuEvent* event)
{
    const int index = tabAt(event->pos());

    QMenu menu(this);
    menu.addAction(tr("Close"),
        [this, index]()
        {
            if (!NX_ASSERT(m_store))
                return;

            if (const auto systemData = m_store->systemData(index))
                disconnectFromSystem(systemData->systemDescription->localId());
        });

    menu.addAction(tr("Open in New Window"),
        [this, index]()
        {
            if (!NX_ASSERT(m_store))
                return;

            if (const auto systemData = m_store->systemData(index))
            {
                const auto logonData = m_stateHandler->adjustedLogonData(systemData->logonData,
                    systemData->systemDescription->localId());

                if (logonData.credentials.authToken.empty())
                {
                    // TODO: VMS-54449: Request missing credentials.
                }

                appContext()->clientStateHandler()->createNewWindow(logonData);
            }
        });

    QnHiDpiWorkarounds::showMenu(&menu, QCursor::pos());
}

void SystemTabBar::disconnectFromSystem(const nx::Uuid& localId)
{
    if (!windowContext()->connectActionsHandler()->askDisconnectConfirmation())
        return;

    const auto state = m_store->state();
    if (state.currentSystemId == localId)
    {
        if (state.systems.size() == 1)
        {
            action(menu::ResourcesModeAction)->setChecked(false);
            menu()->trigger(menu::DisconnectAction);
        }
        else
        {
            const int systemIndex = state.activeSystemTab == state.systems.size() - 1
                ? (state.activeSystemTab - 1)
                : (state.activeSystemTab + 1);

            const auto systemData = state.systems[systemIndex];
            m_stateHandler->connectToSystem(systemData.systemDescription, systemData.logonData);
        }
    }

    m_store->removeSystem(localId);
}

void SystemTabBar::insertClosableTab(int index,
    const core::SystemDescriptionPtr& systemDescription)
{
    const auto text = systemDescription->name();
    insertTab(index, text);
    setTabToolTip(index, text);
    setTabData(index, QVariant::fromValue(systemDescription));

    const auto closeButton = new CloseTabButton(this);
    connect(closeButton, &CloseTabButton::clicked,
        [this, localId = systemDescription->localId()]()
        {
            disconnectFromSystem(localId);
        });

   // closeButton is owned by the tab, so we can use it as a receiver here.
   // This connection is deleted when the tab is removed.
    connect(systemDescription.get(), &core::SystemDescription::systemNameChanged, closeButton,
        [this, systemDescription]()
        {
            const auto data = QVariant::fromValue(systemDescription);
            for (int i = 0; i < count(); ++i)
            {
                if (tabData(i) == data)
                {
                    const auto text = systemDescription->name();
                    const auto fm = fontMetrics();
                    const int tabWidth = kAllDecorationsWidth
                        + fm.size(Qt::TextShowMnemonic, text).width();
                    m_tabWidthCalculator.setWidth(i, tabWidth);

                    setTabText(i, text);
                    setTabToolTip(i, text);
                    break;
                }
            }
        });

    connect(m_stateHandler.get(),
        &StateHandler::activeSystemTabChanged,
        closeButton,
        qOverload<>(&QWidget::update));

    setTabButton(index, RightSide, closeButton);
}

void SystemTabBar::initStyleOption(QStyleOptionTab* option, int tabIndex) const
{
    base_type::initStyleOption(option, tabIndex);
    if (m_store && m_store->isHomeTabActive())
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

void SystemTabBar::at_currentChanged(int index)
{
    if (!NX_ASSERT(m_store))
        return;

    if (const auto systemData = m_store->systemData(index))
    {
        if (systemData->systemDescription->localId() == m_store->currentSystemId())
            action(menu::ResourcesModeAction)->setChecked(true);
        else
            m_stateHandler->connectToSystem(systemData->systemDescription, systemData->logonData);
    }
}

void SystemTabBar::at_tabMoved(int from, int to)
{
    if (!NX_ASSERT(m_store))
        return;

    QScopedValueRollback<bool> guard(m_updating, true);
    m_store->moveSystem(from, to);
    m_tabWidthCalculator.moveTab(from, to);
}

} // namespace nx::vms::client::desktop
