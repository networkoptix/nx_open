#include <algorithm>
#include <nx/utils/log/log.h>
#include "common_update_registry.h"
#include <boost/variant/detail/substitute.hpp>

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
    explicit Deserializer(const QByteArray& rawData):
        m_rawData(rawData)
    {
        m_rootObject = QJsonDocument::fromJson(rawData).object();
        deserializeUrl();
        deserializeMetaData();
        deserializeCustomizationVersionToUpdate();

        if (!m_ok)
            NX_WARNING(this, "UpdateRegistry deserialization failed");
    }

    QString url() const { return m_url; }
    UpdatesMetaData metaData() const { return m_metaData; }
    CustomizationVersionToUpdate customizationVersionToUpdate() const
    {
        return m_customizationVersionToUpdate;
    }
    bool ok() const { return m_ok; }
private:
    const QByteArray& m_rawData;
    QString m_url;
    UpdatesMetaData m_metaData;
    CustomizationVersionToUpdate m_customizationVersionToUpdate;
    bool m_ok = true;
    QJsonObject m_rootObject;

    void deserializeUrl()
    {
        if (!m_rootObject.contains(kUrlKey) || !m_rootObject[kUrlKey].isString())
        {
            m_ok = false;
            return;
        }

        m_url = m_rootObject[kUrlKey].toString();
    }

    void deserializeMetaData()
    {
        if (!m_ok)
            return;

        if (!m_rootObject.contains(kMetaDataKey) || !m_rootObject[kMetaDataKey].isObject())
        {
            m_ok = false;
            return;
        }

        deserializeAlternativeServers();
        deserializeCustomizationDatas();
    }

    void deserializeAlternativeServers()
    {
        QJsonObject metaDataObject = m_rootObject[kMetaDataKey].toObject();
        if (!metaDataObject.contains(kAlternativeServersKey)
            || !metaDataObject[kAlternativeServersKey].isArray())
        {
            m_ok = false;
            return;
        }
        QJsonArray alternativeServersArray = metaDataObject[kAlternativeServersKey].toArray();
        for (int i = 0; i < alternativeServersArray.size(); ++i)
            deserializeAlternativeServerObject(alternativeServersArray[i]);
    }

    void deserializeAlternativeServerObject(const QJsonValue& jsonValue)
    {
        if (!jsonValue.isObject())
        {
            m_ok = false;
            return;
        }

        QJsonObject alternativeServerObject = jsonValue.toObject();
        if (!alternativeServerObject.contains(kUrlKey)
            || !alternativeServerObject[kUrlKey].isString())
        {
            m_ok = false;
            return;
        }

        if (!alternativeServerObject.contains(kAlternativeServerNameKey)
            || !alternativeServerObject[kAlternativeServerNameKey].isString())
        {
            m_ok = false;
            return;
        }

        m_metaData.alternativeServersDataList.append(
            AlternativeServerData(
                alternativeServerObject[kUrlKey].toString(),
                alternativeServerObject[kAlternativeServerNameKey].toString()));
    }

    void deserializeCustomizationDatas()
    {
        if (!m_ok)
            return;

        QJsonObject metaDataObject = m_rootObject[kMetaDataKey].toObject();
        if (!metaDataObject.contains(kCustomizationsKey)
            || !metaDataObject[kCustomizationsKey].isArray())
        {
            m_ok = false;
            return;
        }
        QJsonArray customizationDatasArray = metaDataObject[kCustomizationsKey].toArray();
        for (int i = 0; i < customizationDatasArray.size(); ++i)
            deserializeCustomizationDataObject(customizationDatasArray[i]);
    }

    void deserializeCustomizationDataObject(const QJsonValue& jsonValue)
    {
        if (!jsonValue.isObject())
        {
            m_ok = false;
            return;
        }

        QJsonObject customizationDataObject = jsonValue.toObject();
        if (!customizationDataObject.contains(kCustomizationNameKey)
            || !customizationDataObject[kCustomizationNameKey].isString())
        {
            m_ok = false;
            return;
        }

        if (!customizationDataObject.contains(kVersionsKey)
            || !customizationDataObject[kVersionsKey].isArray())
        {
            m_ok = false;
            return;
        }

        CustomizationData customizationData;
        customizationData.name = customizationDataObject[kCustomizationNameKey].toString();
        fillCustomizationVersions(
            customizationDataObject[kVersionsKey].toArray(),
            customizationData.versions);
        m_metaData.customizationDataList.append(customizationData);
    }

    void fillCustomizationVersions(
        const QJsonArray& customizationDataArray,
        QList<QnSoftwareVersion>& customizationDataList)
    {
        for (int i = 0; i < customizationDataArray.size(); ++i)
            appendCustomizationVersion(customizationDataArray[i], customizationDataList);
    }

    void appendCustomizationVersion(
        const QJsonValue& jsonValue,
        QList<QnSoftwareVersion>& customizationDataList)
    {
        if (!m_ok)
            return;

        if (!jsonValue.isString())
        {
            m_ok = false;
            return;
        }

        customizationDataList.append(QnSoftwareVersion(jsonValue.toString()));
    }

    void deserializeCustomizationVersionToUpdate()
    {
        if (!m_ok)
            return;

        if (!m_rootObject.contains(kCustomizationVersionToUpdateKey)
            || !m_rootObject[kCustomizationVersionToUpdateKey].isArray())
        {
            m_ok = false;
            return;
        }

        deserializeCustomizationVersionToUpdateArray(
            m_rootObject[kCustomizationVersionToUpdateKey].toArray());
    }

    void deserializeCustomizationVersionToUpdateArray(const QJsonArray& jsonArray)
    {
        for (int i = 0; i < jsonArray.size(); ++i)
            insertCustomizationVersionToUpdate(jsonArray[i]);
    }

    void insertCustomizationVersionToUpdate(const QJsonValue& jsonValue)
    {
        if (!m_ok)
            return;

        if (!jsonValue.isObject())
            return;

        QJsonObject customizationVersionToUpdateObject = jsonValue.toObject();
        if (!customizationVersionToUpdateObject.contains(kCustomizationVersionNameKey)
            || !customizationVersionToUpdateObject[kCustomizationVersionNameKey].isString())
        {
            m_ok = false;
            return;
        }

        if (!customizationVersionToUpdateObject.contains(kCustomizationVersionNameKey)
            || !customizationVersionToUpdateObject[kCustomizationVersionNameKey].isString())
        {
            m_ok = false;
            return;
        }

        if (!customizationVersionToUpdateObject.contains(kCustomizationVersionVersionKey)
            || !customizationVersionToUpdateObject[kCustomizationVersionVersionKey].isString())
        {
            m_ok = false;
            return;
        }

        CustomizationVersionData customizationVersionData(
            customizationVersionToUpdateObject[kCustomizationVersionNameKey].toString(),
            QnSoftwareVersion(
                customizationVersionToUpdateObject[kCustomizationVersionVersionKey].toString()));

        if (!customizationVersionToUpdateObject.contains(kUpdateKey)
            || !customizationVersionToUpdateObject[kUpdateKey].isObject())
        {
            m_ok = false;
            return;
        }

        UpdateData updateData;
        if (!deserializeUpdateData(
            customizationVersionToUpdateObject[kUpdateKey].toObject(),
            updateData))
        {
            m_ok = false;
            return;
        }

        m_customizationVersionToUpdate.insert(customizationVersionData, updateData);
    }

    bool deserializeUpdateData(const QJsonObject& updateObject, UpdateData& updateData)
    {
        if (!updateObject.contains(kCloudHostKey)
            || !updateObject[kCloudHostKey].isString())
        {
            m_ok = false;
            return false;
        }

        if (!updateObject.contains(kServerPackagesKey)
            || !updateObject[kServerPackagesKey].isArray())
        {
            m_ok = false;
            return false;
        }

        if (!updateObject.contains(kClientPackagesKey)
            || !updateObject[kClientPackagesKey].isArray())
        {
            m_ok = false;
            return false;
        }

        updateData.cloudHost = updateObject[kCloudHostKey].toString();
        deserializePackages(
            updateObject[kServerPackagesKey].toArray(),
            updateData.targetToPackage);
        deserializePackages(
            updateObject[kClientPackagesKey].toArray(),
            updateData.targetToClientPackage);

        return m_ok;
    }

    void deserializePackages(const QJsonArray& jsonArray, QHash<QString, FileData>& hash)
    {
        for (int i = 0; i < jsonArray.size(); ++i)
            insertValueToTargetToPackage(jsonArray[i], hash);
    }

    void insertValueToTargetToPackage(
        const QJsonValue& jsonValue,
        QHash<QString, FileData>& hash)
    {
        if (!m_ok)
            return;

        if (!jsonValue.isObject())
        {
            m_ok = false;
            return;
        }

        QJsonObject targetToUpdateObject = jsonValue.toObject();
        if (!targetToUpdateObject.contains(kTargetKey)
            || !targetToUpdateObject[kTargetKey].isString())
        {
            m_ok = false;
            return;
        }

        if (!targetToUpdateObject.contains(kFileKey)
            || !targetToUpdateObject[kFileKey].isString())
        {
            m_ok = false;
            return;
        }

        if (!targetToUpdateObject.contains(kUrlKey)
            || !targetToUpdateObject[kUrlKey].isString())
        {
            m_ok = false;
            return;
        }

        if (!targetToUpdateObject.contains(kSizeKey)
            || !targetToUpdateObject[kSizeKey].isDouble())
        {
            m_ok = false;
            return;
        }

        if (!targetToUpdateObject.contains(kMd5Key)
            || !targetToUpdateObject[kMd5Key].isString())
        {
            m_ok = false;
            return;
        }

        hash.insert(
            targetToUpdateObject[kTargetKey].toString(),
            FileData(
                targetToUpdateObject[kFileKey].toString(),
                targetToUpdateObject[kUrlKey].toString(),
                targetToUpdateObject[kSizeKey].toDouble(),
                targetToUpdateObject[kMd5Key].toString().toLatin1()));
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

    return true;
}

bool CommonUpdateRegistry::equals(AbstractUpdateRegistry* other) const
{
    CommonUpdateRegistry* otherCommonUpdateRegistry = dynamic_cast<CommonUpdateRegistry*>(other);
    if (!otherCommonUpdateRegistry)
        return false;

    return otherCommonUpdateRegistry->m_baseUrl == m_baseUrl
        && otherCommonUpdateRegistry->m_metaData == m_metaData
        && otherCommonUpdateRegistry->m_customizationVersionToUpdate == m_customizationVersionToUpdate;
}

} // namespace impl
} // namespace info
} // namespace update
} // namespace nx
