
#include "users_statistics_module.h"

#include <utils/common/model_functions.h>

#include <common/user_permissions.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_user_watcher.h>

namespace
{
    const auto kPermissionsTag = lit("user_permissions");
    QString userNameFromResource(const QnUserResourcePtr &user)
    {
        return QString();//return (user ? user->getName() : QString());
    }
}

QnUsersStatisticsModule::QnUsersStatisticsModule(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
{
    const auto userWatcher = context()->instance<QnWorkbenchUserWatcher>();
    connect(userWatcher, &QnWorkbenchUserWatcher::userAboutToBeChanged
        , this, [](const QnUserResourcePtr &resource)
    {
        qDebug() << "+++++++++ userAboutToBeChanged " << userNameFromResource(resource);
    });

    connect(userWatcher, &QnWorkbenchUserWatcher::userChanged
        , this, [](const QnUserResourcePtr &resource)
    {
        qDebug() << "+++++++++ userChanged " << userNameFromResource(resource);
    });
}

QnUsersStatisticsModule::~QnUsersStatisticsModule()
{}

QnMetricsHash QnUsersStatisticsModule::metrics() const
{
    QnMetricsHash result;


    const auto value = QnLexical::serialized(m_permissions);
    result.insert(kPermissionsTag, value);
    return result;
}

void QnUsersStatisticsModule::resetMetrics()
{
    qDebug() << "+++++++++ resetMetrics";
    m_permissions = Qn::NoPermissions;
}
