
#include "users_statistics_module.h"

#include <nx/fusion/model_functions.h>

#include <core/resource/user_resource.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>

namespace {

const QString kCurrentUserPermissionsTag = "curr_perm";
const QString kCurrentUserRoleTag = "curr_role";
const QString kPermissionCountTagTemplate = "perm_cnt_%1";

} // namespace

QnUsersStatisticsModule::QnUsersStatisticsModule(QObject *parent)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
{
}

QnUsersStatisticsModule::~QnUsersStatisticsModule()
{}

QnStatisticValuesHash QnUsersStatisticsModule::values() const
{

    QnStatisticValuesHash result;

    const auto availableUsers = resourcePool()->getResources<QnUserResource>();
    //NX_ASSERT(!availableUsers.isEmpty(), "Can't gather metrics for empty users list");
    if (availableUsers.isEmpty())
        return result;

    // Adds number of each permission
    typedef QHash<QString, int> PermissionCountHash;
    PermissionCountHash permissionsCount;
    for (const auto &userResource: availableUsers)
    {
        const auto permissions = resourceAccessManager()->globalPermissions(userResource);

        static const auto kDelimiter = L'|';
        const auto permissionsList = QnLexical::serialized(permissions)
            .split(kDelimiter, QString::SkipEmptyParts);
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
    if (!currentUser)
        return result;

    const auto accessController = context()->accessController();
    const auto currentUserPermissions = accessController
        ? accessController->globalPermissions()
        : resourceAccessManager()->globalPermissions(currentUser);
    result.insert(kCurrentUserPermissionsTag, QnLexical::serialized(currentUserPermissions));
    result.insert(kCurrentUserRoleTag, QnLexical::serialized(currentUser->userRole()));

    return result;
}

void QnUsersStatisticsModule::reset()
{
}


