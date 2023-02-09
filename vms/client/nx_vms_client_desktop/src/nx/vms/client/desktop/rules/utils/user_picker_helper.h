// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCoreApplication>

#include <nx/vms/client/desktop/system_context_aware.h>

class QnSubjectValidationPolicy;

namespace nx::vms::client::desktop::rules {

class UserPickerHelper: public SystemContextAware
{
    Q_DECLARE_TR_FUNCTIONS(UserPickerHelper)

public:
    UserPickerHelper(
        SystemContext* context,
        bool acceptAll,
        const QnUuidSet& ids,
        const QnSubjectValidationPolicy* policy,
        bool isIntermediateStateValid = true);

    QString text() const;
    QIcon icon() const;

private:
    bool m_acceptAll{false};
    bool m_isValid{false};
    QnUserResourceList m_users;
    QnUuidList m_roles;
};

} // namespace nx::vms::client::desktop::rules
