// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_tab_bar.h"
#include "system_tab_bar_model.h"
#include "private/close_tab_button.h"

#include <QtCore/QScopedValueRollback>
#include <QtGui/QPainter>
#include <QtWidgets/QAction>
#include <QtWidgets/QToolButton>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_logon/data/logon_data.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/widgets/main_window.h>
#include <ui/workbench/workbench.h>
#include <utils/common/delayed.h>

namespace {
static const int kFixedTabSizeHeight = 32;
static const int kFixedHomeIconWidth = 32;
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

    connect(workbench()->windowContext()->systemTabBarModel(),
        &SystemTabBarModel::dataChanged,
        this,
        &SystemTabBar::rebuildTabs);

    // Home tab
    addTab("");
    auto closeButton = new CloseTabButton(this);
    setTabButton(0, RightSide, closeButton);
    connect(closeButton, &CloseTabButton::clicked, this,
        [this]()
        {
            if (currentIndex() != m_lastTabIndex)
                setCurrentIndex(m_lastTabIndex);
        });
    updateHomeTab();

    connect(this, &QTabBar::currentChanged, this, &SystemTabBar::at_currentTabChanged);
    connect(workbench(),
        &QnWorkbench::currentSystemChanged,
        this,
        &SystemTabBar::at_currentSystemChanged);
    connect(action(ui::action::RemoveSystemFromTabBarAction),
        &QAction::triggered,
        this,
        &SystemTabBar::at_systemDisconnected);
}

void SystemTabBar::rebuildTabs()
{
    auto model = workbench()->windowContext()->systemTabBarModel();
    for (int i = 0; i < model->rowCount(); ++i)
    {
        QModelIndex modelIndex = model->index(i);
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

    while (count() - 1 > model->rowCount())
    {
        removeTab(homeTabIndex() - 1);
    }
}

QSize SystemTabBar::tabSizeHint(int index) const
{
    auto result = base_type::tabSizeHint(index);
    if (isHomeTab(index))
    {
        if (count() == 1 || !isHomeTab(currentIndex()))
            result.setWidth(kFixedHomeIconWidth);
        else
            result.setWidth(kFixedHomeIconWidth * 2);
    }
    else if (result.width() < kFixedHomeIconWidth)
    {
        result.setWidth(kFixedHomeIconWidth);
    }
    return result;
}

void SystemTabBar::at_currentTabChanged(int index)
{
    updateHomeTab();

    if (m_updating)
        return;

    if (isHomeTab(index))
    {
        mainWindow()->setWelcomeScreenVisible(true);
    }
    else
    {
        mainWindow()->setWelcomeScreenVisible(false);
        if (currentIndex() != m_lastTabIndex)
        {
            m_lastTabIndex = index;

            auto modelIndex = workbench()->windowContext()->systemTabBarModel()->index(index);
            auto logonData = modelIndex.data(Qn::LogonDataRole).value<LogonData>();

            appContext()->clientStateHandler()->saveWindowsConfiguration();
            menu()->trigger(ui::action::ConnectAction,
                ui::action::Parameters().withArgument(Qn::LogonDataRole, logonData));
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
    auto systemDescription = tabData(m_lastTabIndex).value<QnSystemDescriptionPtr>();
    workbench()->windowContext()->systemTabBarModel()->removeSystem(systemDescription);
}

void SystemTabBar::addClosableTab(const QString& text,
    const QnSystemDescriptionPtr& systemDescription)
{
    using namespace nx::vms::client::desktop;

    int index = homeTabIndex();

    insertTab(index, text);
    setTabData(index, QVariant::fromValue(systemDescription));

    QAbstractButton *closeButton = new CloseTabButton(this);
    connect(closeButton, &CloseTabButton::clicked,
        [this, systemDescription, index]()
        {
            if (currentIndex() == index)
                menu()->trigger(ui::action::DisconnectAction, {Qn::ForceRole, true});
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
    else if (isHomeTab(currentIndex()))
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
    update();
}

} // namespace nx::vms::client::desktop
