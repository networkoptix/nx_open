#include "search_helper.h"

#include <QtCore/QFileInfo>

#include <common/common_module.h>

#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/user_roles_manager.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/file_layout_resource.h>

namespace nx::vms::client::desktop::resources::search_helper {

QString getUrlForSearch(const QnResourcePtr& resource)
{
    // "layout://" prefix and parameters are removed from URLs of exported layouts.
    // Only filename from local files paths is used.
    if (const auto& aviResource = resource.dynamicCast<QnAviResource>())
    {
        if (const auto storage = aviResource->getStorage())
            return QFileInfo(storage->getUrl()).fileName();
        else
            return QFileInfo(resource->getUrl()).fileName();
    }

    if (const auto& fileLayoutResource = resource.dynamicCast<QnFileLayoutResource>())
        return QFileInfo(resource->getUrl()).fileName();

    // Wearable camera's URL consists of prefix and physical ID, both are unwanted.
    if (resource->flags().testFlag(Qn::wearable_camera))
        return QString();

    return resource->getUrl();
}

bool isSearchStringValid(const QString& searchString)
{
    return !searchString.trimmed().isEmpty();
}

bool matches(const QString& searchString, const QnResourcePtr& resource)
{
    if (!isSearchStringValid(searchString))
        return false;

    auto searchWords = uniqueSearchWords(searchString);
    const auto matches = matchSearchWords(searchWords, resource);

    // Deduce result by loose AND logic: each search word should match at least one resource parameter.
    return matches.keys().size() == searchWords.size();
}

Matches matchSearchWords(const QStringList& searchWords, const QnResourcePtr& resource)
{
    static constexpr int kIdMatchLength = 4;

    Matches result;

    for (const auto& searchWord: searchWords)
    {
        auto checkParameter =
            [&result, &searchWord](Parameter parameter, QString value, int matchLength = 1)
            {
                if (value.compare(searchWord, Qt::CaseInsensitive) == 0)
                    result[searchWord].append(parameter);

                if (searchWord.size() >= matchLength && value.contains(searchWord, Qt::CaseInsensitive))
                    result[searchWord].append(parameter);
            };

        auto checkParameterFullMatch =
            [&result, &searchWord](Parameter parameter, QString value)
            {
                if (value.compare(searchWord, Qt::CaseInsensitive) == 0)
                    result[searchWord].append(parameter);
            };

        if (resource->logicalId() > 0)
            checkParameterFullMatch(Parameter::logicalId, QString::number(resource->logicalId()));
        checkParameter(Parameter::name, resource->getName());
        checkParameter(Parameter::url, getUrlForSearch(resource));

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

QStringList uniqueSearchWords(const QString& searchString)
{
    QStringList result = searchString.split(" ", QString::SkipEmptyParts);
    for (auto& searchWord: result)
        searchWord = searchWord.toLower();
    result.removeDuplicates();
    return result;
}

} // namespace nx::vms::client::desktop::resources::search_helper
