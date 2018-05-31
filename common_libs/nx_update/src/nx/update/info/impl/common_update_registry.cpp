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

class UpdateDataFinder
{
public:
    UpdateDataFinder(
        const UpdateRequestData& updateRequestData,
        const detail::CustomizationVersionToUpdate& customizationVersionToUpdate,
        const CustomizationData& customizationData)
        :
        m_updateRequestData(updateRequestData),
        m_customizationVersionToUpdate(customizationVersionToUpdate),
        m_customizationData(customizationData)
    {
    }

    void process()
    {
        if (!hasNewerVersions(m_updateRequestData.currentNxVersion))
        {
            NX_INFO(
                this,
                lm("No newer versions found. Current version is %1")
                .args(m_updateRequestData.currentNxVersion.toString()));
            return;
        }

        if (!hasRequestedPackage())
        {
            NX_INFO(this, lm("No update package found for %1")
                .args(m_updateRequestData.toString()));
        }

    }

    virtual ~UpdateDataFinder() = default;

    bool ok() const
    {
        return m_found;
    }

    QnSoftwareVersion version() const { return m_version; }

protected:
    const UpdateRequestData& m_updateRequestData;
    const detail::CustomizationVersionToUpdate& m_customizationVersionToUpdate;
    const CustomizationData& m_customizationData;
    mutable bool m_found = false;
    mutable QnSoftwareVersion m_version;

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

        if (checkPackages(*updateIt, version))
            m_version = version;
    }

    virtual bool checkPackages(
        const UpdateData& /*updateData*/,
        const QnSoftwareVersion& /*version*/) const
    {
        m_found = true;
        return true;
    }
};

class FileDataFinder: public UpdateDataFinder
{
public:
    FileDataFinder(
        const QString& baseUrl,
        const UpdateFileRequestData& updateRequestData,
        const detail::CustomizationVersionToUpdate& customizationVersionToUpdate,
        const CustomizationData& customizationData)
        :
        UpdateDataFinder(updateRequestData, customizationVersionToUpdate, customizationData),
        m_baseUrl(baseUrl)
    {
    }

    FileData fileData() const
    {
        return m_fileData;
    }
private:
    const QString& m_baseUrl;
    mutable FileData m_fileData;

    virtual bool checkPackages(
        const UpdateData& updateData,
        const QnSoftwareVersion& version) const override
    {
        for (auto it = updateData.targetToPackage.cbegin();
            it != updateData.targetToPackage.cend();
            ++it)
        {
            checkPackage(it.key(), it.value(), version);
        }

        return m_found;
    }

    void checkPackage(
        const QString& target,
        const FileData& fileData,
        const QnSoftwareVersion& version) const
    {
        if (m_found)
            return;

        if (!static_cast<const UpdateFileRequestData&>(m_updateRequestData).osVersion.matches(target))
            return;

        m_fileData = fileData;
        const auto filePath = QString("/%1/%2/%3").arg(
            m_customizationData.name, QString::number(version.build()), fileData.file);
        m_fileData.url = m_baseUrl + filePath;
        m_found = true;
    }
};

static const QString kUrlKey = "url";
static const QString kManualDataKey = "manualData";
static const QString kRemovedDataKey = "removedData";
static const QString kMetaDataKey = "metaData";
static const QString kPeersKey = "peers";
static const QString kIsClientKey = "isClient";
static const QString kAlternativeServersKey = "alternativeServers";
static const QString kCustomizationKey = "customization";
static const QString kCustomizationsKey = "customizations";
static const QString kAlternativeServerNameKey = "name";
static const QString kAlternativeServerUrlKey = "url";
static const QString kCustomizationNameKey = "name";
static const QString kNxVersionKey = "nxVersion";
static const QString kOsVersionKey = "osVersion";
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
        const CustomizationVersionToUpdate& customizationVersionToUpdate,
        const QList<ManualFileData>& manualData,
        const QMap<QString, QList<QnUuid>>& removedData)
        :
        m_url(url),
        m_metaData(metaData),
        m_customizationVersionToUpdate(customizationVersionToUpdate)
    {
        serializeUrl();
        serializeMetaData();
        serializeManualData(manualData);
        serializeRemovedData(removedData);
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

    void serializeManualData(const QList<ManualFileData>& manualData)
    {
        QJsonArray manualDataArray;
        for (const auto& data: manualData)
        {
            QJsonObject arrayObject;
            arrayObject[kFileKey] = data.file;
            arrayObject[kIsClientKey] = data.isClient;
            arrayObject[kOsVersionKey] = data.osVersion.serialize();
            arrayObject[kNxVersionKey] = data.nxVersion.toString();

            QJsonArray peerArray;
            for (const auto& peerId: data.peers)
                peerArray.append(peerId.toString());

            arrayObject[kPeersKey] = peerArray;
            manualDataArray.append(arrayObject);
        }

        m_jsonObject[kManualDataKey] = manualDataArray;
    }

    void serializeRemovedData(const QMap<QString, QList<QnUuid>>& removedData)
    {
        QJsonObject removedDataMap;
        for (auto removedIt = removedData.cbegin(); removedIt != removedData.cend(); ++removedIt)
        {
            QJsonArray peerArray;
            for (const auto& peer: removedIt.value())
                peerArray.append(peer.toString());

            removedDataMap[removedIt.key()] = peerArray;
        }

        m_jsonObject[kRemovedDataKey] = removedDataMap;
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
        deserializeManualData();
        deserializeRemovedData();
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
    QList<ManualFileData> manualData() const { return m_manualData; }
    QMap<QString, QList<QnUuid>> removedData() const { return m_removedData; }
    bool ok() const { return m_ok; }

private:
    const QByteArray& m_rawData;
    QString m_url;
    UpdatesMetaData m_metaData;
    CustomizationVersionToUpdate m_customizationVersionToUpdate;
    QList<ManualFileData> m_manualData;
    QMap<QString, QList<QnUuid>> m_removedData;
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

    void deserializeManualData()
    {
        if (!m_ok)
            return;

        if (!m_rootObject.contains(kManualDataKey) || !m_rootObject[kManualDataKey].isArray())
        {
            m_ok = false;
            return;
        }

        auto manualDataArray = m_rootObject[kManualDataKey].toArray();
        for (int i = 0; i < manualDataArray.size(); ++i)
        {
            auto arrayObj = manualDataArray[i].toObject();
            ManualFileData manualFileData;
            manualFileData.file = arrayObj[kFileKey].toString();
            manualFileData.isClient = arrayObj[kIsClientKey].toBool();
            manualFileData.nxVersion = QnSoftwareVersion(arrayObj[kNxVersionKey].toString());
            manualFileData.osVersion = OsVersion::deserialize(arrayObj[kOsVersionKey].toString());

            auto peersArrayObj = arrayObj[kPeersKey].toArray();
            for (int j = 0; j < peersArrayObj.size(); ++j)
                manualFileData.peers.append(QnUuid::fromStringSafe(peersArrayObj[j].toString()));

            m_manualData.append(manualFileData);
        }
    }

    void deserializeRemovedData()
    {
        if (!m_ok)
            return;

        if (!m_rootObject.contains(kRemovedDataKey) || !m_rootObject[kRemovedDataKey].isObject())
        {
            m_ok = false;
            return;
        }

        auto removedDataObject = m_rootObject[kRemovedDataKey].toObject();
        for (const auto& key: removedDataObject.keys())
        {
            QList<QnUuid> peers;
            auto peerArray = removedDataObject[key].toArray();
            for (int i = 0; i < peerArray.size(); ++i)
                peers.append(QnUuid::fromStringSafe(peerArray[i].toString()));

            m_removedData.insert(key, peers);
        }
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
    const QnUuid& selfPeerId,
    const QString& baseUrl,
    detail::data_parser::UpdatesMetaData metaData,
    detail::CustomizationVersionToUpdate customizationVersionToUpdate)
    :
    m_peerId(selfPeerId),
    m_baseUrl(baseUrl),
    m_metaData(std::move(metaData)),
    m_customizationVersionToUpdate(std::move(customizationVersionToUpdate))
{}

CommonUpdateRegistry::CommonUpdateRegistry(const QnUuid& selfPeerId): m_peerId(selfPeerId) {}

ResultCode CommonUpdateRegistry::findUpdateFile(
    const UpdateFileRequestData& updateRequestData,
    FileData* outFileData) const
{
    NX_VERBOSE(this, lm("Requested update for %1").args(updateRequestData.toString()));

    for (const auto& manualDataEntry: m_manualData)
    {
        if (manualDataEntry.isClient == updateRequestData.isClient
            && manualDataEntry.nxVersion > updateRequestData.currentNxVersion
            && manualDataEntry.osVersion == updateRequestData.osVersion)
        {
            if (outFileData)
                *outFileData = FileData(manualDataEntry.file, QString(), -1, QByteArray());
            return ResultCode::ok;
        }
    }

    CustomizationData customizationData;
    if (!hasUpdateForCustomizationAndVersion(updateRequestData, &customizationData))
        return ResultCode::noData;

    FileDataFinder fileDataFinder(
        m_baseUrl,
        updateRequestData,
        m_customizationVersionToUpdate,
        customizationData);

    fileDataFinder.process();
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
    CustomizationData* customizationData) const
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
    return Serializer(
        m_baseUrl,
        m_metaData,
        m_customizationVersionToUpdate,
        m_manualData,
        m_removedData).json();
}

bool CommonUpdateRegistry::fromByteArray(const QByteArray& rawData)
{
    Deserializer deserializer(rawData);
    if (!deserializer.ok())
        return false;

    m_baseUrl = deserializer.url();
    m_metaData = deserializer.metaData();
    m_customizationVersionToUpdate = deserializer.customizationVersionToUpdate();
    m_manualData = deserializer.manualData();
    m_removedData = deserializer.removedData();

    return true;
}

bool CommonUpdateRegistry::equals(AbstractUpdateRegistry* other) const
{
    CommonUpdateRegistry* otherCommonUpdateRegistry = dynamic_cast<CommonUpdateRegistry*>(other);
    if (!otherCommonUpdateRegistry)
        return false;

    return otherCommonUpdateRegistry->m_baseUrl == m_baseUrl
        && otherCommonUpdateRegistry->m_metaData == m_metaData
        && otherCommonUpdateRegistry->m_customizationVersionToUpdate == m_customizationVersionToUpdate
        && otherCommonUpdateRegistry->m_manualData == m_manualData
        && otherCommonUpdateRegistry->m_removedData == m_removedData;
}

ResultCode CommonUpdateRegistry::latestUpdate(
    const UpdateRequestData& updateRequestData,
    QnSoftwareVersion* outSoftwareVersion) const
{
    for (const auto& md: m_manualData)
    {
        if (updateRequestData.currentNxVersion < md.nxVersion)
        {
            if (outSoftwareVersion)
                *outSoftwareVersion = md.nxVersion;
            return ResultCode::ok;
        }
    }

    CustomizationData customizationData;
    if (!hasUpdateForCustomizationAndVersion(updateRequestData, &customizationData))
        return ResultCode::noData;

    UpdateDataFinder updateDataFinder(
        updateRequestData,
        m_customizationVersionToUpdate,
        customizationData);

    updateDataFinder.process();
    if (!updateDataFinder.ok())
    {
        NX_VERBOSE(this, lm("No update found for %1").args(updateRequestData.toString()));
        return ResultCode::noData;
    }

    *outSoftwareVersion = updateDataFinder.version();
    return ResultCode::ok;
}

void CommonUpdateRegistry::addFileData(const ManualFileData& manualFileData)
{
    for (const auto& selfManualFileData: m_manualData)
    {
        if (selfManualFileData.file == manualFileData.file)
        {
            if (selfManualFileData.peers.contains(m_peerId))
                return;
        }
    }

    if (m_removedData.contains(manualFileData.file))
        m_removedData[manualFileData.file].removeAll(m_peerId);

    addFileDataImpl(manualFileData, QList<QnUuid>() << m_peerId);
}

void CommonUpdateRegistry::addFileDataImpl(
    const ManualFileData& manualFileData,
    const QList<QnUuid>& peers)
{
    bool sameFileFound = false;
    for (auto& selfManualFileData: m_manualData)
    {
        if (selfManualFileData.file == manualFileData.file)
        {

            for (int j = 0; j < peers.size(); ++j)
            {
                if (!selfManualFileData.peers.contains(peers[j]))
                    selfManualFileData.peers.append(peers[j]);
            }

            sameFileFound = true;
            break;
        }
    }

    if (!sameFileFound)
    {
        auto manualFileDataCopy = manualFileData;
        manualFileDataCopy.peers = peers;
        m_manualData.append(manualFileDataCopy);
    }

    for (auto& selfManualFileData: m_manualData)
    {
        if (selfManualFileData.file == manualFileData.file)
        {
            std::sort(
                selfManualFileData.peers.begin(),
                selfManualFileData.peers.end(),
                [](const QnUuid& l, const QnUuid& r) { return l < r; });
        }
    }

    std::sort(
        m_manualData.begin(),
        m_manualData.end(),
        [](const ManualFileData& l, const ManualFileData& r) { return l.file < r.file; });
}

void CommonUpdateRegistry::removeFileData(const QString& fileName)
{
    if (!removeFileDataImpl(fileName, QList<QnUuid>() << m_peerId))
        return;

    if (m_removedData.contains(fileName))
    {
        if (!m_removedData[fileName].contains(m_peerId))
            m_removedData[fileName].append(m_peerId);
    }
    else
    {
        m_removedData.insert(fileName, QList<QnUuid>() << m_peerId);
    }
}

bool CommonUpdateRegistry::removeFileDataImpl(const QString& fileName, const QList<QnUuid>& peers)
{
    bool result = false;
    for (int i = 0; i < m_manualData.size(); ++i)
    {
        if (m_manualData[i].file == fileName)
        {
            for (int j = 0; j < m_manualData[i].peers.size(); ++j)
            {
                if (peers.contains(m_manualData[i].peers[j]))
                {
                    result = true;
                    m_manualData[i].peers.removeAt(j);
                }
            }

            if (m_manualData[i].peers.isEmpty())
                m_manualData.removeAt(i);

            return result;
        }
    }

    return result;
}

void CommonUpdateRegistry::merge(AbstractUpdateRegistry* other)
{
    auto otherCommonUpdateRegistry = dynamic_cast<CommonUpdateRegistry*>(other);
    if (!otherCommonUpdateRegistry)
        return;

    bool hasNewVersions = false;
    for (const auto& customizationData: m_metaData.customizationDataList)
    {
        for (const auto& otherCustomizationData:
            otherCommonUpdateRegistry->m_metaData.customizationDataList)
        {
            if (otherCustomizationData.name == customizationData.name
                && otherCustomizationData.versions.last() > customizationData.versions.last())
            {
                hasNewVersions = true;
                break;
            }
        }
        if (hasNewVersions)
            break;
    }

    if (hasNewVersions)
    {
        m_metaData = otherCommonUpdateRegistry->m_metaData;
        m_customizationVersionToUpdate = otherCommonUpdateRegistry->m_customizationVersionToUpdate;
        m_baseUrl = otherCommonUpdateRegistry->m_baseUrl;
    }

    for (const auto& otherManualData: otherCommonUpdateRegistry->m_manualData)
        addFileDataImpl(otherManualData, otherManualData.peers);

    for (auto removedIt = otherCommonUpdateRegistry->m_removedData.cbegin();
         removedIt != otherCommonUpdateRegistry->m_removedData.cend();
         ++removedIt)
    {
        auto peers = removedIt.value();
        peers.removeAll(m_peerId);
        removeFileDataImpl(removedIt.key(), peers);
        mergeToRemovedData(removedIt.key(), peers);
    }
}

void CommonUpdateRegistry::mergeToRemovedData(const QString& fileName, const QList<QnUuid>& peers)
{
    if (m_removedData.contains(fileName))
    {
        for (const auto& peer: peers)
        {
            if (!m_removedData[fileName].contains(peer))
                m_removedData[fileName].append(peer);
        }
    }
    else
    {
        m_removedData.insert(fileName, peers);
    }
}

QList<QnUuid> CommonUpdateRegistry::additionalPeers(const QString& fileName) const
{
    for (const auto& md: m_manualData)
    {
        if (md.file == fileName)
            return md.peers;
    }

    return QList<QnUuid>();
}

} // namespace impl
} // namespace info
} // namespace update
} // namespace nx
