
#include "users_statistics_module.h"

#include <utils/common/model_functions.h>

#include <common/user_permissions.h>
#include <core/resource/resource_name.h>
#include <core/resource/user_resource.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <core/resource_management/resource_pool.h>

namespace
{
    const auto kPermissionsTag = lit("curr_perm");
    const auto kPermissionCountTagTemplate = lit("perm_cnt_%1");

    QString extractUserName(const QnResourcePtr &resource)
    {
        if (!resource.dynamicCast<QnUserResource>())
            return QString();
        return getShortResourceName(resource);
    }
}

QnUsersStatisticsModule::QnUsersStatisticsModule(QObject *parent)
    : base_type(parent)
    , m_context()
{
}

QnUsersStatisticsModule::~QnUsersStatisticsModule()
{}

QnMetricsHash QnUsersStatisticsModule::metrics() const
{
    if (!m_context)
        return QnMetricsHash();

    const auto accessController = m_context->accessController();
    if (!accessController)
        return QnMetricsHash();

    QnMetricsHash result;

    const auto availableUsers = qnResPool->getResources<QnUserResource>();
    Q_ASSERT_X(!availableUsers.isEmpty(), Q_FUNC_INFO, "Can't gather metrics for empty users list");

    // Adds number of each permission
    typedef QHash<QString, int> PermissionCountHash;
    PermissionCountHash permissionsCount;
    for (const auto &userResource: availableUsers)
    {
        const auto permissions = accessController->globalPermissions(userResource);

        static const auto kDelimieter = L'|';
        const auto permissionsList = QnLexical::serialized(permissions)
            .split(kDelimieter, QString::SkipEmptyParts);
        for (auto permissionStr: permissionsList)
            ++permissionsCount[permissionStr];
    }

    for (auto it = permissionsCount.begin(); it != permissionsCount.end(); ++it)
    {
        const auto name = it.key();
        const auto count = it.value();
        result.insert(kPermissionCountTagTemplate.arg(name), QString::number(count));
    }

    // Adds current user permissions metric
    const auto currentUser = m_context->user();
    Q_ASSERT_X(!currentUser, Q_FUNC_INFO, "There is no current user!");
    if (!currentUser)
        return result;

    const auto currentUserPermissions = accessController->globalPermissions();
    const auto value = QnLexical::serialized(currentUserPermissions);
    result.insert(kPermissionsTag, value);

    return result;
}

void QnUsersStatisticsModule::resetMetrics()
{
}

void QnUsersStatisticsModule::setContext(QnWorkbenchContext *context)
{
    m_context = context;
}


