// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_management_tab_widget.h"

#include <functional>

#include <QtWidgets/QVBoxLayout>

#include <nx/vms/client/desktop/common/widgets/tab_widget.h>
#include <nx/vms/client/desktop/style/custom_style.h>

#include "ldap_settings_widget.h"
#include "user_groups_widget.h"
#include "user_list_widget.h"

namespace nx::vms::client::desktop {

struct UserManagementTabWidget::Private
{
    UserManagementTabWidget* const q;
    TabWidget* const tabWidget{new TabWidget(q)};

    void forEachTab(std::function<void(QnAbstractPreferencesWidget*)> functor) const
    {
        for (int i = 0; i < tabWidget->count(); ++i)
        {
            const auto tab = qobject_cast<QnAbstractPreferencesWidget*>(tabWidget->widget(i));
            if (NX_ASSERT(tab))
                functor(tab);
        }
    }
};

UserManagementTabWidget::UserManagementTabWidget(QWidget* parent):
    base_type(parent),
    d(new Private{.q = this})
{
    const auto userListWidget = new UserListWidget(d->tabWidget);
    userListWidget->layout()->setContentsMargins(0, 12, 0, 0);

    const auto userGroupsWidget = new UserGroupsWidget(d->tabWidget);

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
}

UserManagementTabWidget::~UserManagementTabWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void UserManagementTabWidget::loadDataToUi()
{
    d->forEachTab([](QnAbstractPreferencesWidget* tab) { tab->loadDataToUi(); });
}

void UserManagementTabWidget::applyChanges()
{
    d->forEachTab([](QnAbstractPreferencesWidget* tab) { tab->applyChanges(); });
}

bool UserManagementTabWidget::hasChanges() const
{
    bool result = false;
    d->forEachTab(
        [&result](QnAbstractPreferencesWidget* tab) { result = result || tab->hasChanges(); });
    return result;
}

void UserManagementTabWidget::manageDigestUsers()
{
    constexpr int kUserListTab = 0;
    d->tabWidget->setCurrentIndex(kUserListTab);

    const auto userList = qobject_cast<UserListWidget*>(d->tabWidget->currentWidget());
    if (NX_ASSERT(userList))
        userList->filterDigestUsers();
}

} // namespace nx::vms::client::desktop
