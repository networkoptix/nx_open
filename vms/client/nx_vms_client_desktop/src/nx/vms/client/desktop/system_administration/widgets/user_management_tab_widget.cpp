// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_management_tab_widget.h"

#include <functional>

#include <range/v3/algorithm/any_of.hpp>

#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/common/widgets/tab_widget.h>
#include <nx/vms/client/desktop/style/custom_style.h>

#include "ldap_settings_widget.h"
#include "user_groups_widget.h"
#include "user_list_widget.h"

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

struct UserManagementTabWidget::Private
{
    UserManagementTabWidget* const q;
    TabWidget* const tabWidget{new TabWidget(q)};

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

UserManagementTabWidget::UserManagementTabWidget(UserGroupManager* manager, QWidget* parent):
    base_type(parent),
    d(new Private{.q = this})
{
    const auto userListWidget = new UserListWidget(d->tabWidget);
    const auto userGroupsWidget = new UserGroupsWidget(manager, d->tabWidget);
    const auto ldapSettingsWidget = new LdapSettingsWidget(d->tabWidget);

    setTabShape(d->tabWidget->tabBar(), style::TabShape::Compact);

    d->tabWidget->setProperty(style::Properties::kTabBarIndent,
        style::Metrics::kDefaultTopLevelMargin);

    d->tabWidget->addTab(userListWidget, tr("Users"));
    d->tabWidget->addTab(userGroupsWidget, tr("Groups"));
    d->tabWidget->addTab(ldapSettingsWidget, tr("LDAP"));

    const auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(d->tabWidget);

    for (auto tab: d->tabs())
    {
        connect(tab, &QnAbstractPreferencesWidget::hasChangesChanged, this,
            &QnAbstractPreferencesWidget::hasChangesChanged);
    }
}

UserManagementTabWidget::~UserManagementTabWidget()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
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
    return ranges::any_of(d->tabs(), [](auto tab) { return tab->hasChanges(); });
}

void UserManagementTabWidget::discardChanges()
{
    for (auto tab: d->tabs())
        tab->discardChanges();
    NX_ASSERT(!isNetworkRequestRunning());
}

bool UserManagementTabWidget::isNetworkRequestRunning() const
{
    return ranges::any_of(d->tabs(), [](auto tab) { return tab->isNetworkRequestRunning(); });
}

void UserManagementTabWidget::manageDigestUsers()
{
    constexpr int kUserListTab = 0;
    d->tabWidget->setCurrentIndex(kUserListTab);

    const auto userList = qobject_cast<UserListWidget*>(d->tabWidget->currentWidget());
    if (NX_ASSERT(userList))
        userList->filterDigestUsers();
}

void UserManagementTabWidget::resetWarnings()
{
    for (auto tab: d->tabs())
        tab->resetWarnings();
}

} // namespace nx::vms::client::desktop
