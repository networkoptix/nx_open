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
    static constexpr int kIdMatchLength = 4;

    Matches result;

    if (!isSearchStringValid(searchString))
        return result;

    // Combining search values with AND logic.
    QStringList searchWords = searchString.split(" ", QString::SkipEmptyParts);
    for (const auto& searchWord: searchWords)
    {
        auto checkParameter =
            [&result, &searchWord](Parameter parameter, QString value, int matchLength = 1)
            {
                if (value.compare(searchWord, Qt::CaseInsensitive) == 0)
                    result.emplace(parameter, value);

                if (searchWord.size() >= matchLength && value.contains(searchWord, Qt::CaseInsensitive))
                    result.emplace(parameter, value);
            };

        auto checkParameterFullMatch =
            [&result, &searchWord](Parameter parameter, QString value)
            {
                if (value.compare(searchWord, Qt::CaseInsensitive) == 0)
                    result.emplace(parameter, value);
            };

        if (resource->logicalId() > 0)
            checkParameterFullMatch(Parameter::logicalId, QString::number(resource->logicalId()));
        checkParameter(Parameter::name, resource->getName());
        checkParameter(Parameter::url, resource->getUrl());

        if (const auto& camera = resource.dynamicCast<QnVirtualCameraResource>())
        {
            // Check ids by at least 4 character substring match, both with and without braces.
            checkParameter(Parameter::id, camera->getId().toSimpleString(), kIdMatchLength);
            checkParameter(Parameter::id, camera->getId().toString(), kIdMatchLength);
            checkParameter(Parameter::mac, camera->getMAC().toString());
            checkParameter(Parameter::host, camera->getHostAddress());
            checkParameter(Parameter::model, camera->getModel());
            checkParameter(Parameter::vendor, camera->getVendor());
            checkParameter(Parameter::firmware, camera->getFirmware());
            checkParameter(Parameter::physicalId, camera->getPhysicalId(), kIdMatchLength);
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
