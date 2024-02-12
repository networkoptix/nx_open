// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <nx/vms/api/data/user_group_data.h>
#include <nx/vms/client/desktop/system_context_aware.h>

class QnSubjectValidationPolicy;

namespace nx::vms::client::desktop::rules {

struct UserPickerHelperParameters
{
    size_t additionalUsers{0};
    size_t additionalValidUsers{0};
};

class UserPickerHelper: public SystemContextAware
{
    Q_DECLARE_TR_FUNCTIONS(UserPickerHelper)

public:
    UserPickerHelper(
        SystemContext* context,
        bool acceptAll,
        const UuidSet& ids,
        const QnSubjectValidationPolicy* policy,
        bool isIntermediateStateValid = true,
        const UserPickerHelperParameters& parameters = {});

    QString text() const;
    QIcon icon() const;

private:
    bool m_acceptAll{false};
    bool m_isValid{false};
    QnUserResourceList m_users;
    api::UserGroupDataList m_groups;
    UserPickerHelperParameters m_parameters;
};

} // namespace nx::vms::client::desktop::rules
