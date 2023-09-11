// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_picker_helper.h"

#include <QtGui/QIcon>

#include <business/business_resource_validation.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/unicode_chars.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

using namespace nx::vms::api;

namespace nx::vms::client::desktop::rules {

namespace {

QString makeText(const QString& base, const QString& additional)
{
    if (base.isEmpty())
        return additional;

    if (additional.isEmpty())
        return base;

    return QString{"%1, %2"}.arg(base).arg(additional);
}

} // namespace

UserPickerHelper::UserPickerHelper(
    SystemContext* context,
    bool acceptAll,
    const QnUuidSet& ids,
    const QnSubjectValidationPolicy* policy,
    bool isIntermediateStateValid,
    const UserPickerHelperParameters& parameters)
    :
    SystemContextAware{context},
    m_acceptAll{acceptAll},
    m_parameters{parameters}
{
    NX_ASSERT(m_parameters.additionalUsers >= m_parameters.additionalValidUsers);

    nx::vms::common::getUsersAndGroups(context, ids, m_users, m_groups);

    if (!policy->isEmptySelectionAllowed() && !acceptAll && ids.empty())
    {
        m_isValid = m_parameters.additionalUsers > 0
            && m_parameters.additionalValidUsers == m_parameters.additionalUsers;
    }
    else
    {
        const auto validity = policy->validity(acceptAll, ids);
        const auto isAdditionalValid =
            m_parameters.additionalValidUsers == m_parameters.additionalUsers;
        m_isValid = (validity == QValidator::Acceptable && isAdditionalValid)
            || (validity == QValidator::Intermediate && isIntermediateStateValid);
    }
}

QString UserPickerHelper::text() const
{
    const auto additional = m_parameters.additionalUsers == 0
        ? QString{}
        : tr("%n additional", "", m_parameters.additionalUsers);

    if (m_acceptAll)
        return makeText(tr("All Users"), additional);

    if (m_users.empty() && m_groups.empty())
        return m_parameters.additionalUsers == 0 ? tr("Select at least one user") : additional;

    if (m_users.size() == 1 && m_groups.empty())
        return makeText(m_users.front()->getName(), additional);

    if (m_users.empty() && m_groups.size() <= 2)
    {
        QStringList groupNames;
        for (const auto& group: m_groups)
            groupNames.push_back(std::move(group.name));
        groupNames.sort(Qt::CaseInsensitive);

        return groupNames.join(", ");
    }

    if (m_groups.empty())
        return makeText(tr("%n Users", "", m_users.size()), additional);

    if (!m_users.empty())
    {
        const auto usersText = QString("%1, %2")
            .arg(tr("%n Groups", "", m_groups.size()))
            .arg(tr("%n Users", "", m_users.size()));
        return makeText(usersText, additional);
    }

    return makeText(tr("%n Groups", "", m_groups.size()), additional);
}

QIcon UserPickerHelper::icon() const
{
    static const QColor mainColor = "#A5B7C0";
    static const nx::vms::client::core::SvgIconColorer::IconSubstitutions colorSubs = {
        {QnIcon::Normal, {{mainColor, "light10"}}}};

    if (m_acceptAll)
    {
        return core::Skin::maximumSizePixmap(m_isValid
                ? qnSkin->icon("tree/users.svg", colorSubs)
                : qnSkin->icon("tree/users_alert.svg", colorSubs),
            QIcon::Selected,
            QIcon::Off,
            /*correctDevicePixelRatio*/ false);
    }

    if (m_users.isEmpty() && m_groups.empty() && m_parameters.additionalUsers == 0)
        return qnSkin->icon("tree/user_alert.svg");

    const auto users = m_users.size() + m_parameters.additionalUsers;
    const bool multiple = users > 1 || !m_groups.empty();
    const QIcon icon = qnSkin->icon(multiple
        ? (m_isValid ? "tree/users.svg" : "tree/users_alert.svg")
        : (m_isValid ? "tree/user.svg" : "tree/user_alert.svg"), colorSubs);

    return core::Skin::maximumSizePixmap(
        icon,
        QIcon::Selected,
        QIcon::Off,
        /*correctDevicePixelRatio*/ false);
}

} // namespace nx::vms::client::desktop::rules
