// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_picker_helper.h"

#include <QtGui/QIcon>

#include <business/business_resource_validation.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/utils/qset.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>

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
    const auto validity = policy->validity(acceptAll, ids);
    m_isValid = validity == QValidator::Acceptable
        || (validity == QValidator::Intermediate && isIntermediateStateValid);
}

QString UserPickerHelper::text() const
{
    if (m_acceptAll)
        return tr("All Users");

    if (m_users.empty() && m_roles.empty())
        return tr("Select at least one user");

    if (m_users.size() == 1 && m_roles.empty())
        return m_users.front()->getName();

    if (m_users.empty() && m_roles.size() == 1)
    {
        return QString("%1 %2 %3")
            .arg(tr("Role"))
            .arg(QChar(0x2013)) //< En-dash.
            .arg(systemContext()->userRolesManager()->userRoleName(m_roles.front()));
    }

    if (m_roles.empty())
        return tr("%n Users", "", m_users.size());

    if (!m_users.empty())
    {
        return QString("%1, %2")
            .arg(tr("%n Roles", "", m_roles.size()))
            .arg(tr("%n Users", "", m_users.size()));
    }

    static const auto kAdminRoles = nx::utils::toQSet(QnPredefinedUserRoles::adminIds());
    if (nx::utils::toQSet(m_roles) == kAdminRoles)
        return tr("All Administrators");

    return tr("%n Roles", "", m_roles.size());
}

QIcon UserPickerHelper::icon() const
{
    if (m_acceptAll)
    {
        return Skin::maximumSizePixmap(
            m_isValid ? qnSkin->icon("tree/users.svg") : qnSkin->icon("tree/users_alert.svg"),
            QIcon::Selected,
            QIcon::Off,
            /*correctDevicePixelRatio*/ false);
    }

    if (m_users.isEmpty() && m_roles.isEmpty())
        return qnSkin->icon("tree/user_alert.svg");

    const bool multiple = m_users.size() > 1 || !m_roles.empty();
    const QIcon icon = qnSkin->icon(multiple
        ? (m_isValid ? "tree/users.svg" : "tree/users_alert.svg")
        : (m_isValid ? "tree/user.svg" : "tree/user_alert.svg"));

    return Skin::maximumSizePixmap(
        icon,
        QIcon::Selected,
        QIcon::Off,
        /*correctDevicePixelRatio*/ false);
}

} // namespace nx::vms::client::desktop::rules
