#include <algorithm>
#include <nx/utils/log/log.h>
#include "common_update_registry.h"

namespace nx {
namespace update {
namespace info {
namespace impl {

namespace {

using namespace detail;
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

static const QString kUrlKey = "url";
static const QString kMetaDataKey = "metaData";
static const QString kAlternativeServersKey = "alternativeServers";
static const QString kCustomizationsKey = "customizations";
static const QString kAlternativeServerNameKey = "name";
static const QString kAlternativeServerUrlKey = "url";
static const QString kCustomizationNameKey = "name";
static const QString kVersionsKey = "versions";
static const QString kCustomizationVersionToUpdateKey = "customizationVersionToUpdate";
static const QString kCustomizationVersionNameKey = "customizationName";
static const QString kCustomizationVersionVersionKey = "customizationVersion";
static const QString kUpdateKey = "update";
static const QString kCloudHostKey = "cloudHost";
static const QString kServerPackagesKey = "serverPackages";
static const QString kClientPackagesKey = "clientPackages";
static const QString kTargetKey = "target";
static const QString kFileKey = "file";
static const QString kSizeKey = "size";
static const QString kMd5Key = "md5";


class Serializer
{
public:
    Serializer(
        const QString& url,
        const UpdatesMetaData& metaData,
        const CustomizationVersionToUpdate& customizationVersionToUpdate)
        :
        m_url(url),
        m_metaData(metaData),
        m_customizationVersionToUpdate(customizationVersionToUpdate)
    {
        serializeUrl();
        serializeMetaData();
        serializeCustomizationVersionToUpdate();
    }

    QByteArray json() const { return QJsonDocument(m_jsonObject).toJson(); }
private:
    const QString& m_url;
    const UpdatesMetaData& m_metaData;
    const CustomizationVersionToUpdate& m_customizationVersionToUpdate;
    QJsonObject m_jsonObject;

    void serializeUrl()
    {
        m_jsonObject[kUrlKey] = m_url;
    }

    void serializeMetaData()
    {
        QJsonObject metaDataObject;
        serializeAlternativeServers(metaDataObject);
        serializeCustomizations(metaDataObject);
        m_jsonObject[kMetaDataKey] = metaDataObject;
    }

    void serializeAlternativeServers(QJsonObject& metaDataObject) const
    {
        QJsonArray alternativeServersArray;
        for (const auto& alternativeServer: m_metaData.alternativeServersDataList)
        {
            QJsonObject alternativeServerObject;
            alternativeServerObject[kAlternativeServerNameKey] = alternativeServer.name;
            alternativeServerObject[kAlternativeServerUrlKey] = alternativeServer.url;
            alternativeServersArray.append(alternativeServerObject);
        }
        metaDataObject[kAlternativeServersKey] = alternativeServersArray;
    }

    void serializeCustomizations(QJsonObject& metaDataObject) const
    {
        QJsonArray customizationsArray;
        for (const auto& customizationData: m_metaData.customizationDataList)
        {
            QJsonObject customizationDataObject;
            customizationDataObject[kCustomizationNameKey] = customizationData.name;
            fillCustomizationDataVersions(customizationData, customizationDataObject);
            customizationsArray.append(customizationDataObject);
        }
        metaDataObject[kCustomizationsKey] = customizationsArray;
    }

    static void fillCustomizationDataVersions(
        const CustomizationData& customizationData,
        QJsonObject& customizationDataObject)
    {
        QJsonArray versionsArray;
        for (const auto& version : customizationData.versions)
            versionsArray.append(QJsonValue(version.toString()));

        customizationDataObject[kVersionsKey] = versionsArray;
    }


    void serializeCustomizationVersionToUpdate()
    {
        QJsonArray customizationVersionToUpdateArray;
        for (auto it = m_customizationVersionToUpdate.cbegin();
            it != m_customizationVersionToUpdate.cend();
            ++it)
        {
            QJsonObject customizationVersionToUpdateObject;
            customizationVersionToUpdateObject[kCustomizationVersionNameKey] = it.key().name;
            customizationVersionToUpdateObject[kCustomizationVersionVersionKey] =
                it.key().version.toString();
            serializeUpdate(it.value(), customizationVersionToUpdateObject);
            customizationVersionToUpdateArray.append(customizationVersionToUpdateObject);
        }
        m_jsonObject[kCustomizationVersionToUpdateKey] = customizationVersionToUpdateArray;
    }

    static void serializeUpdate(
        const UpdateData& updateData,
        QJsonObject& customizationVersionToUpdateObject)
    {
        QJsonObject updateObject;
        updateObject[kCloudHostKey] = updateData.cloudHost;
        serializePackages(updateData.targetToPackage, updateObject, kServerPackagesKey);
        serializePackages(updateData.targetToClientPackage, updateObject, kClientPackagesKey);
        customizationVersionToUpdateObject[kUpdateKey] = updateObject;
    }

    static void serializePackages(
        const QHash<QString, FileData>& targetToPackage,
        QJsonObject& updateObject,
        const QString& packagesKey)
    {
        QJsonArray targetToPackageArray;
        for (auto it = targetToPackage.cbegin(); it != targetToPackage.cend(); ++it)
        {
            QJsonObject targetToPackageObject;
            targetToPackageObject[kTargetKey] = it.key();
            serializeFileData(it.value(), targetToPackageObject);
            targetToPackageArray.append(targetToPackageObject);
        }
        updateObject[packagesKey] = targetToPackageArray;
    }

    static void serializeFileData(const FileData& fileData, QJsonObject& targetToPackageObject)
    {
        targetToPackageObject[kFileKey] = fileData.file;
        targetToPackageObject[kUrlKey] = fileData.url;
        targetToPackageObject[kSizeKey] = fileData.size;
        targetToPackageObject[kMd5Key] = QString::fromLatin1(fileData.md5);
    }
};

class Deserializer
{
public:
    explicit Deserializer(const QByteArray& rawData)
    {
        QJsonObject rootObject = QJsonDocument::fromJson(rawData).object();
    }

    QString url() const { return m_url; }
    UpdatesMetaData metaData() const { return m_metaData; }
    CustomizationVersionToUpdate customizationVersionToUpdate() const
    {
        return m_customizationVersionToUpdate;
    }
    bool ok() const { return m_ok; }
private:
    QString m_url;
    UpdatesMetaData m_metaData;
    CustomizationVersionToUpdate m_customizationVersionToUpdate;
    QJsonObject m_jsonObject;
    bool m_ok = false;
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
    return Serializer(m_baseUrl, m_metaData, m_customizationVersionToUpdate).json();
}

bool CommonUpdateRegistry::fromByteArray(const QByteArray& rawData)
{
    Deserializer deserializer(rawData);
    if (!deserializer.ok())
        return false;

    m_baseUrl = deserializer.url();
    m_metaData = deserializer.metaData();
    m_customizationVersionToUpdate = deserializer.customizationVersionToUpdate();

    return false;
}

bool CommonUpdateRegistry::equals(AbstractUpdateRegistry* other) const
{
    return false;
}

} // namespace impl
} // namespace info
} // namespace update
} // namespace nx
