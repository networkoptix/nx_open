// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_tab_bar.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>
#include <QtWidgets/QToolButton>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/data/logon_data.h>
#include <nx/vms/client/desktop/system_logon/ui/welcome_screen.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/common/indents.h>
#include <ui/widgets/main_window.h>
#include <utils/common/delayed.h>

#include "private/close_tab_button.h"
#include "system_tab_bar_model.h"

namespace {
static const int kFixedTabSizeHeight = 32;
static const int kFixedHomeIconWidth = 32;
static const int kFixedWideHomeIconWidth = 65;
static const QnIndents kTabIndents = {14, 10};
} // namespace

namespace nx::vms::client::desktop {

SystemTabBar::SystemTabBar(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    setMovable(false);
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    setDrawBase(false);
    setTabsClosable(false);
    setElideMode(Qt::ElideRight);
    setTabShape(this, nx::style::TabShape::Rectangular);
    setUsesScrollButtons(false);
    setFixedHeight(kFixedTabSizeHeight);
    setContentsMargins(1, 0, 1, 0);
    setProperty(style::Properties::kSideIndentation, QVariant::fromValue(kTabIndents));

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

    connect(model(), &SystemTabBarModel::dataChanged, this, &SystemTabBar::rebuildTabs);

    // Home tab
    addTab("");
    auto closeButton = new CloseTabButton(this);
    setTabButton(0, RightSide, closeButton);
    connect(closeButton, &CloseTabButton::clicked, this,
        [this]()
        {
            mainWindow()->setWelcomeScreenVisible(false);
        });
    updateHomeTab();

    connect(this, &QTabBar::currentChanged, this, &SystemTabBar::at_currentTabChanged);
    connect(workbench(),
        &Workbench::currentSystemChanged,
        this,
        &SystemTabBar::at_currentSystemChanged);
    connect(action(menu::RemoveSystemFromTabBarAction),
        &QAction::triggered,
        this,
        &SystemTabBar::at_systemDisconnected);
    connect(mainWindow()->welcomeScreen(), &WelcomeScreen::globalPreloaderVisibleChanged, this,
        [this](bool visible)
        {
            setEnabled(!visible);
        });
}

void SystemTabBar::rebuildTabs()
{
    for (int i = 0; i < model()->rowCount(); ++i)
    {
        QModelIndex modelIndex = model()->index(i);
        auto text = modelIndex.data(Qt::DisplayRole).toString();
        const auto systemDescription = modelIndex.data(Qn::SystemDescriptionRole);
        if (i < homeTabIndex())
        {
            setTabText(i, text);
            setTabData(i, systemDescription);
        }
        else
        {
            addClosableTab(text, systemDescription.value<QnSystemDescriptionPtr>());
        }
    }

    while (count() - 1 > model()->rowCount())
        removeTab(homeTabIndex() - 1);
}

void SystemTabBar::activateHomeTab()
{
    setCurrentIndex(homeTabIndex());
}

void SystemTabBar::activatePreviousTab()
{
    if (m_updating)
        return;

    const auto systemId = systemContext()->localSystemId();
    if (systemId.isNull())
        setCurrentIndex(m_lastTabIndex);
    else if (const auto index = model()->findSystem(systemId); index.isValid())
        setCurrentIndex(index.row());
}

bool SystemTabBar::isHomeTabActive() const
{
    return isHomeTab(currentIndex());
}

bool SystemTabBar::isUpdating() const
{
    return m_updating;
}

QSize SystemTabBar::tabSizeHint(int index) const
{
    int width;
    if (isHomeTab(index))
    {
        if (count() == 1 || !isHomeTabActive())
            width = kFixedHomeIconWidth;
        else
            width = kFixedWideHomeIconWidth;
    }
    else
    {
        auto size = base_type::tabSizeHint(index);
        width = size.width() + style::Metrics::kStandardPadding;
        if (width < kFixedHomeIconWidth)
            width = kFixedHomeIconWidth;
    }
    return QSize(width, kFixedTabSizeHeight);
}

void SystemTabBar::at_currentTabChanged(int index)
{
    updateHomeTab();

    if (m_updating)
        return;

    QModelIndex modelIndex;
    if (!isHomeTab(m_lastTabIndex))
    {
        modelIndex = model()->index(m_lastTabIndex);
        if (modelIndex.isValid())
        {
            WorkbenchState workbenchState;
            workbench()->submit(workbenchState, /*forceIncludeEmptyLayouts*/ true);
            model()->setData(
                modelIndex, QVariant::fromValue(workbenchState), Qn::WorkbenchStateRole);
        }
    }
    m_lastTabIndex = index;

    if (isHomeTab(index))
    {
        mainWindow()->setWelcomeScreenVisible(true);
    }
    else
    {
        QScopedValueRollback<bool> guard(m_updating, true);
        mainWindow()->setWelcomeScreenVisible(false);
        modelIndex = model()->index(index);
        const auto systemId = systemContext()->localSystemId();
        if (modelIndex.data(Qn::LocalSystemIdRole).value<QString>() != systemId.toSimpleString())
        {
            auto logonData = modelIndex.data(Qn::LogonDataRole).value<LogonData>();
            logonData.storePassword = true;
            menu()->trigger(menu::ConnectAction,
                menu::Parameters().withArgument(Qn::LogonDataRole, logonData));
        }
    }
}

void SystemTabBar::at_currentSystemChanged(QnSystemDescriptionPtr systemDescription)
{
    for (int i = 0; i < count(); ++i)
    {
        if (i == currentIndex())
            continue;

        if (tabData(i).value<QnSystemDescriptionPtr>() == systemDescription)
        {
            QScopedValueRollback<bool> guard(m_updating, true);
            setCurrentIndex(i);
            m_lastTabIndex = i;
            break;
        }
    }
}

void SystemTabBar::at_systemDisconnected()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto systemId = parameters.argument(Qn::LocalSystemIdRole).value<QnUuid>();
    workbench()->removeSystem(systemId);
}

void SystemTabBar::addClosableTab(const QString& text,
    const QnSystemDescriptionPtr& systemDescription)
{
    using namespace nx::vms::client::desktop;

    int index = homeTabIndex();

    insertTab(index, text);
    setTabData(index, QVariant::fromValue(systemDescription));

    QAbstractButton* closeButton = new CloseTabButton(this);
    connect(closeButton, &CloseTabButton::clicked,
        [this, systemDescription, index]()
        {
            if (currentIndex() == index)
                menu()->trigger(menu::DisconnectAction, {Qn::ForceRole, true});
            else
                workbench()->removeSystem(systemDescription);
    });
    setTabButton(index, RightSide, closeButton);
}

bool SystemTabBar::isHomeTab(int index) const
{
    return index == homeTabIndex();
}

int SystemTabBar::homeTabIndex() const
{
    return count() - 1;
}

void SystemTabBar::updateHomeTab()
{
    const int index = homeTabIndex();
    auto closeButton = tabButton(index, RightSide);
    if (count() == 1)
    {
        // Only home tab exists.
        closeButton->setVisible(false);
        setTabIcon(index, qnSkin->icon("titlebar/systems.svg"));
    }
    else if (isHomeTabActive())
    {
        // Home tab is active.
        closeButton->setVisible(true);
        setTabIcon(index, qnSkin->icon("titlebar/systems.svg"));
    }
    else
    {
        // Home tab is inactive.
        closeButton->setVisible(false);
        setTabIcon(index, qnSkin->icon("titlebar/add.svg"));
    }
    resizeEvent({});
    updateGeometry();
}

SystemTabBarModel* SystemTabBar::model() const
{
    return workbench()->windowContext()->systemTabBarModel();
}

} // namespace nx::vms::client::desktop
