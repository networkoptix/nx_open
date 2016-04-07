
#include "users_statistics_module.h"

#include <utils/common/model_functions.h>

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
    , QnWorkbenchContextAware(parent)
{
}

QnUsersStatisticsModule::~QnUsersStatisticsModule()
{}

QnStatisticValuesHash QnUsersStatisticsModule::values() const
{
    const auto accessController = context()->accessController();
    if (!accessController)
        return QnStatisticValuesHash();

    QnStatisticValuesHash result;

    const auto availableUsers = qnResPool->getResources<QnUserResource>();
    //NX_ASSERT(!availableUsers.isEmpty(), Q_FUNC_INFO, "Can't gather metrics for empty users list");
    if (availableUsers.isEmpty())
        return result;

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
    const auto currentUser = context()->user();
    NX_ASSERT(currentUser, Q_FUNC_INFO, "There is no current user!");
    if (!currentUser)
        return result;

    const auto currentUserPermissions = accessController->globalPermissions();
    const auto value = QnLexical::serialized(currentUserPermissions);
    result.insert(kPermissionsTag, value);

    return result;
}

void QnUsersStatisticsModule::reset()
{
}


