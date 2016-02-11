
#include "users_statistics_module.h"

#include <common/user_permissions.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_user_watcher.h>

namespace
{
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
    qDebug() << "+++++++++ metrics";
    return QnMetricsHash();
}

void QnUsersStatisticsModule::resetMetrics()
{
    qDebug() << "+++++++++ resetMetrics";
    m_permissions = Qn::NoPermissions;
}
