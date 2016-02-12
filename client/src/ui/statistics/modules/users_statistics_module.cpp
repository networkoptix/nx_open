
#include "users_statistics_module.h"

#include <utils/common/model_functions.h>

#include <common/user_permissions.h>
#include <core/resource/resource_name.h>
#include <core/resource/user_resource.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_user_watcher.h>

namespace
{
    const auto kPermissionsTag = lit("permissions");
}

QnUsersStatisticsModule::QnUsersStatisticsModule(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
{
    const auto userWatcher = context()->instance<QnWorkbenchUserWatcher>();

    connect(userWatcher, &QnWorkbenchUserWatcher::userChanged
        , this, [this](const QnUserResourcePtr &user)
    {
        if (!user)
            resetMetrics();
        else
            m_permissions = user->getPermissions();
    });
}

QnUsersStatisticsModule::~QnUsersStatisticsModule()
{}

QnMetricsHash QnUsersStatisticsModule::metrics() const
{
    QnMetricsHash result;
    const auto value = QnLexical::serialized(
        static_cast<Qn::Permissions>(m_permissions));
    result.insert(kPermissionsTag, value);
    return result;
}

void QnUsersStatisticsModule::resetMetrics()
{
    m_permissions = Qn::NoPermissions;
}
