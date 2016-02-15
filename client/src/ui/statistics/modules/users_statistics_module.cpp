
#include "users_statistics_module.h"

#include <utils/common/model_functions.h>

#include <common/user_permissions.h>
#include <core/resource/resource_name.h>
#include <core/resource/user_resource.h>
#include <ui/workbench/workbench_context.h>
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
    , m_currentUserName()
{
    connect(context(), &QnWorkbenchContext::userChanged
        , this, [this](const QnUserResourcePtr &userResource)
    {
        m_currentUserName = extractUserName(userResource);
    });
}

QnUsersStatisticsModule::~QnUsersStatisticsModule()
{}

QnMetricsHash QnUsersStatisticsModule::metrics() const
{
    const auto availableUsers = qnResPool->getResources<QnUserResource>();

    Q_ASSERT_X(!availableUsers.isEmpty(), Q_FUNC_INFO, "Can't gather metrics for empty users list");
    Q_ASSERT_X(!m_currentUserName.isEmpty(), Q_FUNC_INFO, "There is no current user!");

    typedef QHash<QString, int> PermissionCountHash;
    PermissionCountHash permissionsCount;

    Qn::Permissions currentUserPermissions = Qn::NoPermissions;
    for (const auto &userResource: availableUsers)
    {
        const auto permissions = static_cast<Qn::Permissions>(
            userResource->getPermissions());

        if (extractUserName(userResource) == m_currentUserName)
            currentUserPermissions = permissions;

        static const auto kDelimieter = L'|';
        const auto permissionsList = QnLexical::serialized(permissions)
            .split(kDelimieter, QString::SkipEmptyParts);
        for (auto permissionStr: permissionsList)
            ++permissionsCount[permissionStr];
    }

    QnMetricsHash result;

    // Adds current user permissions metric
    if (!m_currentUserName.isEmpty())
    {
        const auto value = QnLexical::serialized(currentUserPermissions);
        result.insert(kPermissionsTag, value);
    }

    // Adds number of each permission
    for (auto it = permissionsCount.begin(); it != permissionsCount.end(); ++it)
    {
        const auto name = it.key();
        const auto count = it.value();
        result.insert(kPermissionCountTagTemplate.arg(name), QString::number(count));
    }

    return result;
}

void QnUsersStatisticsModule::resetMetrics()
{
    m_currentUserName.clear();
}

