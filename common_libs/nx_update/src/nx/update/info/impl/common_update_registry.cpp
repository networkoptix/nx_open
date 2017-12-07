#include <algorithm>
#include <nx/utils/log/log.h>
#include "common_update_registry.h"

namespace nx {
namespace update {
namespace info {
namespace impl {

namespace {

using namespace detail::data_parser;

class FileDataFinder
{
public:
    FileDataFinder(
        const QString& baseUrl,
        const UpdateRequestData& updateRequestData,
        const detail::CustomizationVersionToUpdate& customizationVersionToUpdate,
        const CustomizationData& customizationData)
        :
        m_baseUrl(baseUrl),
        m_updateRequestData(updateRequestData),
        m_customizationVersionToUpdate(customizationVersionToUpdate),
        m_customizationData(customizationData)
    {
        if (!hasNewerVersions(updateRequestData.currentNxVersion))
        {
            NX_WARNING(
                this,
                lm("No newer versions found. Current version is %1")
                    .args(updateRequestData.currentNxVersion.toString()));
            return;
        }

        if (!hasRequestedPackage())
        {
            NX_WARNING(this, lm("No update package found for %1")
                .args(updateRequestData.toString()));
        }
    }

    bool ok() const
    {
        return m_found;
    }

    FileData fileData() const
    {
        return m_fileData;
    }
private:
    const QString& m_baseUrl;
    const UpdateRequestData& m_updateRequestData;
    const detail::CustomizationVersionToUpdate& m_customizationVersionToUpdate;
    const CustomizationData& m_customizationData;
    mutable FileData m_fileData;
    mutable bool m_found = false;

    bool hasNewerVersions(const QnSoftwareVersion& currentNxVersion) const
    {
        if (m_customizationData.versions.last() <= currentNxVersion)
            return false;

        return true;
    }

    bool hasRequestedPackage() const
    {
        for (auto versionIt = m_customizationData.versions.rbegin();
            versionIt != m_customizationData.versions.rend();
            ++versionIt)
        {
            checkForUpdateData(*versionIt);
        }

        if (!m_found)
            return false;

        return true;
    }

    void checkForUpdateData(const QnSoftwareVersion& version) const
    {
        if (m_found)
            return;

        if (version <= m_updateRequestData.currentNxVersion)
            return;

        auto updateIt = m_customizationVersionToUpdate.find(
            detail::CustomizationVersionData(m_customizationData.name, version));
        if (updateIt == m_customizationVersionToUpdate.cend())
            return;

        if (updateIt->cloudHost != m_updateRequestData.cloudHost)
        {
            NX_WARNING(this, lm("Cloud host doesn't match: %1").args(updateIt->cloudHost));
            return;
        }

        for (auto packageIt = updateIt->targetToPackage.cbegin();
            packageIt != updateIt->targetToPackage.cend();
            ++packageIt)
        {
            checkPackage(packageIt.key(), packageIt.value(), version);
        }
    }

    void checkPackage(
        const QString& target,
        const FileData& fileData,
        const QnSoftwareVersion& version) const
    {
        if (m_found)
            return;

        if (!m_updateRequestData.osVersion.matches(target))
            return;

        m_fileData = fileData;
        auto filePath = lit("/%1/%2/%3")
            .arg(m_customizationData.name)
            .arg(QString::number(version.build()))
            .arg(fileData.file);
        m_fileData.url = m_baseUrl + filePath;
        m_found = true;
    }
};
} // namespace

CommonUpdateRegistry::CommonUpdateRegistry(
    const QString& baseUrl,
    detail::data_parser::UpdatesMetaData metaData,
    detail::CustomizationVersionToUpdate customizationVersionToUpdate)
    :
    m_baseUrl(baseUrl),
    m_metaData(std::move(metaData)),
    m_customizationVersionToUpdate(std::move(customizationVersionToUpdate))
{}

ResultCode CommonUpdateRegistry::findUpdate(
    const UpdateRequestData& updateRequestData,
    FileData* outFileData)
{
    NX_VERBOSE(this, lm("Requested update for %1").args(updateRequestData.toString()));

    CustomizationData customizationData;
    if (!hasUpdateForCustomizationAndVersion(updateRequestData, &customizationData))
        return ResultCode::noData;

    FileDataFinder fileDataFinder(
        m_baseUrl,
        updateRequestData,
        m_customizationVersionToUpdate,
        customizationData);

    if (!fileDataFinder.ok())
        return ResultCode::noData;

    *outFileData = fileDataFinder.fileData();
    NX_INFO(this, lm("Update for %1 successfully found").args(updateRequestData.toString()));

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
    const UpdateRequestData& updateRequestData,
    CustomizationData* customizationData)
{
    using namespace detail::data_parser;

    auto customizationIt = std::find_if(
        m_metaData.customizationDataList.cbegin(),
        m_metaData.customizationDataList.cend(),
        [&updateRequestData](const CustomizationData& customizationData)
        {
            return customizationData.name == updateRequestData.customization;
        });

    if (customizationIt == m_metaData.customizationDataList.cend())
    {
        NX_WARNING(
            this,
            lm("No update for this customization %1").args(updateRequestData.customization));
        return false;
    }
    *customizationData = *customizationIt;

    return true;
}

QByteArray CommonUpdateRegistry::toByteArray() const
{
    return QByteArray();
}

bool CommonUpdateRegistry::fromByteArray(const QByteArray& rawData)
{
    return false;
}

} // namespace impl
} // namespace info
} // namespace update
} // namespace nx
