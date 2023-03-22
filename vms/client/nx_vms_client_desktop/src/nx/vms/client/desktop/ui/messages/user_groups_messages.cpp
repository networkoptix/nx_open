// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_groups_messages.h"

#include <QtWidgets/QListWidget>

#include <core/resource_management/user_roles_manager.h>
#include <nx/vms/client/desktop/settings/show_once_settings.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_context.h>

namespace {

static constexpr int kMaximumRows = 10;
static constexpr int kRecommendedWidth = 284;

class GroupListWidget: public QListWidget
{
    using base_type = QListWidget;

public:
    using base_type::base_type;

    QSize sizeHint() const
    {
        return QSize(kRecommendedWidth,
            nx::style::Metrics::kViewRowHeight * std::min(kMaximumRows, (int) model()->rowCount()));
    }
};

QWidget* createGroupListWidget(QnSessionAwareMessageBox* parent, const QSet<QnUuid>& groups)
{
    auto groupList = new GroupListWidget(parent);

    groupList->setFocusPolicy(Qt::NoFocus);
    groupList->setSelectionMode(QAbstractItemView::NoSelection);
    groupList->setProperty(nx::style::Properties::kSuppressHoverPropery, true);
    groupList->setMaximumHeight(nx::style::Metrics::kViewRowHeight);

    const auto customIcon = qnSkin->pixmap("user_settings/group_custom.svg");

    for (const auto& id: groups)
    {
        const auto groupData =
            parent->context()->systemContext()->userRolesManager()->userRole(id);
        if (groupData.id.isNull())
            continue;

        auto item = new QListWidgetItem(groupData.name, groupList);
        item->setFlags({Qt::ItemIsEnabled});
        item->setIcon(customIcon);
    }

    groupList->setMaximumHeight(groupList->sizeHint().height());

    return groupList;
}

} // namespace

namespace nx::vms::client::desktop::ui::messages {

bool UserGroups::removeGroups(QWidget* parent, const QSet<QnUuid>& groups)
{
    if (showOnceSettings()->deleteUserGroups())
        return true;

    const QString text = groups.size() == 1
        ? tr("Delete group?")
        : tr("Delete %n group(s)?", "", groups.size());

    QnSessionAwareMessageBox messageBox(parent);
    messageBox.setIcon(QnMessageBoxIcon::Question);
    messageBox.setText(text);

    messageBox.setStandardButtons(QDialogButtonBox::Cancel);
    messageBox.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    messageBox.addCustomWidget(createGroupListWidget(&messageBox, groups));

    messageBox.setCheckBoxEnabled();

    const auto result = messageBox.exec();
    if (messageBox.isChecked())
        showOnceSettings()->deleteUserGroups = true;

    return result != QDialogButtonBox::Cancel;
}

} // nx::vms::client::desktop::ui::messages
