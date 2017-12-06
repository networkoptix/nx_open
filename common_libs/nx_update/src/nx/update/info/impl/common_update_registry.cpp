#include <algorithm>
#include <nx/utils/log/log.h>
#include "common_update_registry.h"

namespace nx {
namespace update {
namespace info {
namespace impl {

CommonUpdateRegistry::CommonUpdateRegistry(
    detail::data_parser::UpdatesMetaData metaData,
    detail::CustomizationVersionToUpdate customizationVersionToUpdate)
    :
    m_metaData(std::move(metaData)),
    m_customizationVersionToUpdate(std::move(customizationVersionToUpdate))
{}

ResultCode CommonUpdateRegistry::findUpdate(
    const UpdateRequestData& updateRequestData,
    FileData* outFileData)
{
    NX_VERBOSE(this, lm("Requested update for %1").args(updateRequestData.toString()));

    if (!hasUpdateForCustomizationAndVersion(updateRequestData))
        return ResultCode::noData;

    if (!hasRequestedPackage(updateRequestData))
        return ResultCode::noData;

    NX_INFO(this, lm("Update for %1 successfully found").args(updateRequestData.toString()));
    *outFileData = *m_fileData;

    return ResultCode::ok;
}

QList<QString> CommonUpdateRegistry::alternativeServers() const
{
    QList<QString> result;
    for (const auto& serverData: m_metaData.alternativeServersDataList)
        result.append(serverData.url);
    return result;
}

bool CommonUpdateRegistry::hasUpdateForCustomizationAndVersion(
    const UpdateRequestData& updateRequestData)
{
    using namespace detail::data_parser;

    m_customizationIt = m_metaData.customizationDataList.cend();
    m_customizationIt = std::find_if(
        m_metaData.customizationDataList.cbegin(),
        m_metaData.customizationDataList.cend(),
        [&updateRequestData](const CustomizationData& customizationData)
        {
            return customizationData.name == updateRequestData.customization;
        });

    if (m_customizationIt == m_metaData.customizationDataList.cend())
    {
        NX_WARNING(
            this,
            lm("No update for this customization %1").args(updateRequestData.customization));
        return false;
    }

    return hasNewerVersions(updateRequestData.currentNxVersion);
}

bool CommonUpdateRegistry::hasNewerVersions(const QnSoftwareVersion& nxVersion) const
{
    if (m_customizationIt->versions.last() <= nxVersion)
    {
        NX_WARNING(
            this,
            lm("No newer versions found. Current version is %1").args(nxVersion.toString()));
        return false;
    }

    return true;
}

bool CommonUpdateRegistry::hasRequestedPackage(const UpdateRequestData& updateRequestData)
{
    m_fileData = nullptr;
    for (auto versionIt = m_customizationIt->versions.cbegin();
        versionIt != m_customizationIt->versions.cend();
        ++versionIt)
    {
        checkPackage(updateRequestData, *versionIt);
    }

    if (m_fileData == nullptr)
    {
        NX_WARNING( this, lm("No package found."));
        return false;
    }

    return true;
}

void CommonUpdateRegistry::checkPackage(
    const UpdateRequestData& updateRequestData,
    const QnSoftwareVersion& version)
{
    if (m_fileData != nullptr)
        return;

    if (version <= updateRequestData.currentNxVersion)
        return;

    // todo: implement
}

} // namespace impl
} // namespace info
} // namespace update
} // namespace nx
