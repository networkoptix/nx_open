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
    if (!hasCustomization(updateRequestData.customization))
    {
        NX_WARNING(
            this,
            lm("No update for this customization %1").args(updateRequestData.customization));
        return ResultCode::noData;
    }

    if (!hasUpdateForOs(updateRequestData.osVersion))
    {
        NX_WARNING(
            this,
            lm("No update for this os %1.%2.%3").args(
                updateRequestData.osVersion.family,
                updateRequestData.osVersion.architecture,
                updateRequestData.osVersion.version));
        return ResultCode::noData;
    }

    return hasUpdateForVersionAndCloudHost(updateRequestData);
}

QList<QString> CommonUpdateRegistry::alternativeServers() const
{
    QList<QString> result;
    for (const auto& serverData: m_metaData.alternativeServersDataList)
        result.append(serverData.url);
    return result;
}

bool CommonUpdateRegistry::hasCustomization(const QString& customization)
{
    auto it = std::find_if(
        m_metaData.customizationDataList.cbegin(),
        m_metaData.customizationDataList.cend(),
        [&customization](const detail::data_parser::CustomizationData& customizationData)
        {
            return customizationData.name == customization;
        });

    return it != m_metaData.customizationDataList.cend();
}

bool CommonUpdateRegistry::hasUpdateForOs(const OsVersion& osVersion)
{
    // todo: implement
    return true;
}

ResultCode CommonUpdateRegistry::hasUpdateForVersionAndCloudHost(
    const UpdateRequestData& updateRequestData)
{
    return ResultCode::noData;
}


} // namespace impl
} // namespace info
} // namespace update
} // namespace nx
