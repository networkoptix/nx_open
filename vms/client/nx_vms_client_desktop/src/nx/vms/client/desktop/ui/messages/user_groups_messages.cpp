// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_groups_messages.h"

#include <QtWidgets/QListWidget>

#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <ui/common/indents.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_context.h>

namespace {

static constexpr int kMaximumRows = 10;
static constexpr int kRecommendedWidth = 284;

class GroupListWidget: public QListWidget
{
public:
    GroupListWidget(QWidget* parent = nullptr):
        QListWidget(parent)
    {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setProperty(nx::style::Properties::kSideIndentation, QVariant::fromValue(QnIndents{0, 0}));
    }

    QSize sizeHint() const
    {
        return QSize(kRecommendedWidth,
            nx::style::Metrics::kViewRowHeight * std::min(kMaximumRows, (int)model()->rowCount()));
    }
};

QIcon iconForGroup(const nx::vms::api::UserGroupData& group)
{
    switch (group.type)
    {
        case nx::vms::api::UserType::local:
            return qnSkin->pixmap("user_settings/group_custom.svg");

        case nx::vms::api::UserType::ldap:
            return qnSkin->pixmap("user_settings/group_ldap.svg");

        case nx::vms::api::UserType::cloud:
        default:
            return {};
    }
}

QWidget* createGroupListWidget(QnSessionAwareMessageBox* parent, const QSet<QnUuid>& groups)
{
    auto groupList = new GroupListWidget(parent);

    groupList->setFocusPolicy(Qt::NoFocus);
    groupList->setSelectionMode(QAbstractItemView::NoSelection);
    groupList->setProperty(nx::style::Properties::kSuppressHoverPropery, true);
    groupList->setMaximumHeight(nx::style::Metrics::kViewRowHeight);

    for (const auto& id: groups)
    {
        const auto groupData = parent->system()->userGroupManager()->find(id);
        if (!groupData)
            continue;

        auto item = new QListWidgetItem(groupData->name, groupList);
        item->setFlags({Qt::ItemIsEnabled});
        item->setIcon(iconForGroup(*groupData));
    }

    groupList->setMaximumHeight(groupList->sizeHint().height());

    return groupList;
}

} // namespace

namespace nx::vms::client::desktop::ui::messages {

bool UserGroups::removeGroups(QWidget* parent, const QSet<QnUuid>& groups, bool allowSilent)
{
    if (allowSilent)
    {
        if (showOnceSettings()->deleteUserGroups())
            return true;
    }

    const QString text = groups.size() == 1
        ? tr("Delete group?")
        : tr("Delete %n groups?", "", groups.size());

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(text);

    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    messageBox.addCustomWidget(createGroupListWidget(&messageBox, groups),
        QnMessageBox::Layout::AfterMainLabel);

    if (allowSilent)
        messageBox.setCheckBoxEnabled();

    const auto result = messageBox.exec();

    if (allowSilent)
    {
        if (messageBox.isChecked())
            showOnceSettings()->deleteUserGroups = true;
    }

    return result != QDialogButtonBox::Cancel;
}

} // nx::vms::client::desktop::ui::messages
