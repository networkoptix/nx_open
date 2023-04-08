// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_picker_helper.h"

#include <QtGui/QIcon>

#include <business/business_resource_validation.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

using namespace nx::vms::api;

namespace nx::vms::client::desktop::rules {

UserPickerHelper::UserPickerHelper(
    SystemContext* context,
    bool acceptAll,
    const QnUuidSet& ids,
    const QnSubjectValidationPolicy* policy,
    bool isIntermediateStateValid)
    :
    SystemContextAware{context},
    m_acceptAll{acceptAll}
{
    nx::vms::common::getUsersAndGroups(context, ids, m_users, m_groups);
    const auto validity = policy->validity(acceptAll, ids);
    m_isValid = validity == QValidator::Acceptable
        || (validity == QValidator::Intermediate && isIntermediateStateValid);
}

QString UserPickerHelper::text() const
{
    if (m_acceptAll)
        return tr("All Users");

    if (m_users.empty() && m_groups.empty())
        return tr("Select at least one user");

    if (m_users.size() == 1 && m_groups.empty())
        return m_users.front()->getName();

    if (m_users.empty() && m_groups.size() == 1)
    {
        return QString("%1 %2 %3")
            .arg(tr("Group"))
            .arg(QChar(0x2013)) //< En-dash.
            .arg(m_groups.front().name);
    }

    if (m_groups.empty())
        return tr("%n Users", "", m_users.size());

    if (!m_users.empty())
    {
        return QString("%1, %2")
            .arg(tr("%n Groups", "", m_groups.size()))
            .arg(tr("%n Users", "", m_users.size()));
    }

    if (m_groups.size() == kAdminGroupIds.size() && std::all_of(m_groups.cbegin(), m_groups.cend(),
        [](const api::UserGroupData& group) { return kAdminGroupIds.contains(group.id); }))
    {
        return tr("All Administrators");
    }

    return tr("%n Groups", "", m_groups.size());
}

QIcon UserPickerHelper::icon() const
{
    if (m_acceptAll)
    {
        return core::Skin::maximumSizePixmap(
            m_isValid ? qnSkin->icon("tree/users.svg") : qnSkin->icon("tree/users_alert.svg"),
            QIcon::Selected,
            QIcon::Off,
            /*correctDevicePixelRatio*/ false);
    }

    if (m_users.isEmpty() && m_groups.empty())
        return qnSkin->icon("tree/user_alert.svg");

    const bool multiple = m_users.size() > 1 || !m_groups.empty();
    const QIcon icon = qnSkin->icon(multiple
        ? (m_isValid ? "tree/users.svg" : "tree/users_alert.svg")
        : (m_isValid ? "tree/user.svg" : "tree/user_alert.svg"));

    return core::Skin::maximumSizePixmap(
        icon,
        QIcon::Selected,
        QIcon::Off,
        /*correctDevicePixelRatio*/ false);
}

} // namespace nx::vms::client::desktop::rules
