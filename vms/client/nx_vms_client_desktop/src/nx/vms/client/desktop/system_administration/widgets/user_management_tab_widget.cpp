// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_management_tab_widget.h"

#include <algorithm>

#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/desktop/common/widgets/tab_widget.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/saas/saas_service_manager.h>

#include "ldap_settings_widget.h"
#include "user_groups_widget.h"
#include "user_list_widget.h"

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

struct UserManagementTabWidget::Private
{
    UserManagementTabWidget* const q;
    TabWidget* const tabWidget{new TabWidget(q)};
    int ldapTabIndex = 0;

    QList<QnAbstractPreferencesWidget*> tabs() const
    {
        QList<QnAbstractPreferencesWidget*> result;
        for (int i = 0; i < tabWidget->count(); ++i)
        {
            const auto tab = qobject_cast<QnAbstractPreferencesWidget*>(tabWidget->widget(i));
            if (NX_ASSERT(tab))
                result.push_back(tab);
        }
        return result;
    }
};

UserManagementTabWidget::UserManagementTabWidget(
    WindowContext* context,
    QWidget* parent)
    :
    base_type(parent),
    WindowContextAware(context),
    d(new Private{.q = this})
{
    const auto userListWidget = new UserListWidget(d->tabWidget);
    const auto userGroupsWidget = new UserGroupsWidget(system()->userGroupManager(), d->tabWidget);
    const auto ldapSettingsWidget = new LdapSettingsWidget(d->tabWidget);

    setTabShape(d->tabWidget->tabBar(), style::TabShape::Compact);

    d->tabWidget->setProperty(style::Properties::kTabBarIndent,
        style::Metrics::kDefaultTopLevelMargin);

    d->tabWidget->addTab(userListWidget, tr("Users"));
    d->tabWidget->addTab(userGroupsWidget, tr("Groups"));
    d->ldapTabIndex = d->tabWidget->addTab(ldapSettingsWidget, tr("LDAP"));

    const auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(d->tabWidget);

    for (auto tab: d->tabs())
    {
        connect(tab, &QnAbstractPreferencesWidget::hasChangesChanged, this,
            &QnAbstractPreferencesWidget::hasChangesChanged);
    }

    connect(system()->saasServiceManager(),
        &saas::ServiceManager::dataChanged,
        this,
        &UserManagementTabWidget::updateTabVisibility);

    updateTabVisibility();
}

UserManagementTabWidget::~UserManagementTabWidget()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

void UserManagementTabWidget::updateTabVisibility()
{
    const auto saas = system()->saasServiceManager();
    d->tabWidget->setTabVisible(d->ldapTabIndex,
        saas->hasFeature(nx::vms::api::SaasTierLimitName::ldapAllowed));
}

void UserManagementTabWidget::loadDataToUi()
{
    for (auto tab: d->tabs())
        tab->loadDataToUi();
}

void UserManagementTabWidget::applyChanges()
{
    for (auto tab: d->tabs())
        tab->applyChanges();
}

bool UserManagementTabWidget::hasChanges() const
{
    return std::ranges::any_of(d->tabs(), [](auto tab) { return tab->hasChanges(); });
}

void UserManagementTabWidget::discardChanges()
{
    for (auto tab: d->tabs())
        tab->discardChanges();
    NX_ASSERT(!isNetworkRequestRunning());
}

bool UserManagementTabWidget::isNetworkRequestRunning() const
{
    return std::ranges::any_of(d->tabs(), [](auto tab) { return tab->isNetworkRequestRunning(); });
}

void UserManagementTabWidget::manageDigestUsers()
{
    constexpr int kUserListTab = 0;
    d->tabWidget->setCurrentIndex(kUserListTab);

    const auto userList = qobject_cast<UserListWidget*>(d->tabWidget->currentWidget());
    if (NX_ASSERT(userList))
        userList->filterDigestUsers();
}

void UserManagementTabWidget::showEvent(QShowEvent*)
{
    d->tabWidget->setFocus();
}

void UserManagementTabWidget::resetWarnings()
{
    for (auto tab: d->tabs())
        tab->resetWarnings();
}

} // namespace nx::vms::client::desktop
