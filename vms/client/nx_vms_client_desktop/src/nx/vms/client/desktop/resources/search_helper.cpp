#include "search_helper.h"

#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/user_roles_manager.h>

namespace nx::vms::client::desktop::resources::search_helper {

bool isSearchStringValid(const QString& searchString)
{
    return !searchString.trimmed().isEmpty();
}

bool matches(const QString& searchString, const QnResourcePtr& resource)
{
    const auto mv = matchValues(searchString, resource);
#ifdef RESOURCE_SEARCH_DEBUG
    qDebug() << "search " << searchString << " in " << resource->getName();
    for (auto [k, v]: mv)
    {
        qDebug() << "key" << (int)k << " value " << v;
    }
    if (!mv.empty())
        qDebug() << "----MATCH!----------";
    else
        qDebug() << "----FAIL!----------";
#endif
    return !mv.empty();
}

Matches matchValues(const QString& searchString, const QnResourcePtr& resource)
{
    const int idMatchingSubstrSize = 4;

    Matches result;

    if (!isSearchStringValid(searchString))
        return result;

    // Combining search values with AND logic.
    QStringList values = searchString.toLower().split(" ", QString::SkipEmptyParts);
    for (const auto& searchValue: values)
    {
        auto checkParameter =
            [&result, &searchValue](Parameter parameter, QString value, int matchingSubstrSize = 1)
            {
                if (value.size() < matchingSubstrSize && searchValue == value.toLower())
                    result.emplace(parameter, value);

                if (value.size() >= matchingSubstrSize && searchValue.size() >= matchingSubstrSize
                    && value.toLower().contains(searchValue))
                {
                    result.emplace(parameter, value);
                }
            };

        if (resource->logicalId() > 0)
            checkParameter(Parameter::logicalId, QString::number(resource->logicalId()),
                idMatchingSubstrSize);
        checkParameter(Parameter::name, resource->getName());
        checkParameter(Parameter::url, resource->getUrl());

        if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
        {
            // Check ids by at least 4 character substring match, both with and without braces.
            checkParameter(Parameter::id, camera->getId().toSimpleString(), idMatchingSubstrSize);
            checkParameter(Parameter::id, camera->getId().toString(), idMatchingSubstrSize);
            checkParameter(Parameter::mac, camera->getMAC().toString());
            checkParameter(Parameter::host, camera->getHostAddress());
            checkParameter(Parameter::model, camera->getModel());
            checkParameter(Parameter::vendor, camera->getVendor());
            checkParameter(Parameter::firmware, camera->getFirmware());
            checkParameter(Parameter::physicalId, camera->getPhysicalId(), idMatchingSubstrSize);
        }

        if (const auto& user = resource.dynamicCast<QnUserResource>())
        {
            const QString roleName = user->commonModule()
                ? user->commonModule()->userRolesManager()->userRoleName(user)
                : QnUserRolesManager::userRoleName(user->userRole());
            checkParameter(Parameter::roleName, roleName);
        }
    }

    return result;
}

} // namespace nx::vms::client::desktop::resources::search_helper
