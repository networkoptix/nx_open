#include <QJsonObject>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <utils/common/software_version.h>

#include "json_data_parser.h"
#include "../update_data.h"
#include "../updates_meta_data.h"

namespace nx {
namespace update {
namespace info {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    FileData, (json),
    (file)(size)(md5))

namespace detail {
namespace data_parser {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AlternativeServerData, (json),
    (url)(name))

namespace impl {

namespace {

struct CustomizationInfo
{
    QString current_release;
    QString updates_prefix;
    QString release_notes;
    QString description;
    QMap<QString, QnSoftwareVersion> releases;
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CustomizationInfo, (json),
    (current_release)(updates_prefix)(release_notes)(description)(releases))

const static QString kAlternativesServersKey = "__info";
const static QString kVersionKey = "version";
const static QString kCloudHostKey = "cloudHost";
const static QString kServerPackagesKey = "packages";
const static QString kClientPackagesKey = "clientPackages";

class MetaDataParser
{
public:
    MetaDataParser(const QJsonObject& topLevelObject):
        m_topLevelObject(topLevelObject)
    {
        NX_ASSERT(!topLevelObject.isEmpty());
        if (topLevelObject.isEmpty())
        {
            NX_WARNING(this, "Top level JSON is empty");
            m_ok = false;
            return;
        }

        // todo: implement parsing information about unsupported versions
        parseAlternativesServerData();
        parseCustomizationData();
    }

    bool ok() const
    {
        return m_ok;
    }

    UpdatesMetaData take()
    {
        return std::move(m_updatesMetaData);
    }

private:
    QJsonObject m_topLevelObject;
    UpdatesMetaData m_updatesMetaData;
    bool m_ok = true;

    void parseAlternativesServerData()
    {
        if (QJson::deserialize(
                m_topLevelObject,
                kAlternativesServersKey,
                &m_updatesMetaData.alternativeServersDataList))
        {
            return;
        }

        NX_ERROR(this, "Alternative server list deserialization failed");
        m_ok = false;
    }

    void parseCustomizationData()
    {
        if (!m_ok)
            return;

        QJsonObject::const_iterator it = m_topLevelObject.constBegin();
        for (; it != m_topLevelObject.constEnd(); ++it)
            parseCustomizationObj(it.key());
    }

    void parseCustomizationObj(const QString& key)
    {
        if (!m_ok)
            return;

        if (key == kAlternativesServersKey)
            return;

        CustomizationInfo customizationInfo;
        if (!QJson::deserialize(m_topLevelObject, key, &customizationInfo))
        {
            NX_ERROR(this, lm("Customization json parsing failed: %1").args(key));
            m_ok = false;
            return;
        }

        CustomizationData customizationData = dataFromInfo(key, std::move(customizationInfo));
        m_updatesMetaData.customizationDataList.append(customizationData);
    }

    CustomizationData dataFromInfo(
        const QString& customizationName,
        CustomizationInfo customizationInfo)
    {
        CustomizationData customizationData;
        customizationData.name = customizationName;

        for (auto releasesIt = customizationInfo.releases.constBegin();
             releasesIt != customizationInfo.releases.constEnd();
             ++releasesIt)
        {
            customizationData.versions.append(releasesIt.value());
        }
        std::sort(customizationData.versions.begin(), customizationData.versions.end());

        return customizationData;
    }
};

class UpdateDataParser
{
public:
    UpdateDataParser(const QJsonObject& topLevelObject):
        m_topLevelObject(topLevelObject)
    {
        NX_ASSERT(!topLevelObject.isEmpty());
        if (topLevelObject.isEmpty())
        {
            NX_WARNING(this, "Top level JSON is empty");
            m_ok = false;
            return;
        }

        parseVersionAndCloudHost();
        parsePackages(kServerPackagesKey);
        parsePackages(kClientPackagesKey);
    }

    bool ok() const { return m_ok; }
    UpdateData take() { return std::move(m_updateData); }

private:
    class PackagesParser
    {
    public:
        struct FileDataWithTarget
        {
            FileData fileData;
            QString targetName;

            FileDataWithTarget(FileData fileData, QString targetName):
                fileData(std::move(fileData)),
                targetName(std::move(targetName))
            {}
        };

        PackagesParser(const QString& packagesHeader, QJsonObject packagesObject):
            m_packagesObject(packagesObject)
        {
            if (m_packagesObject.isEmpty())
            {
                NX_WARNING(
                    this,
                    lm("No targets OS in packages for %1").args(packagesHeader));
                return;
            }

            parsePackagesObjects();
        }

        bool ok() const { return m_ok; }
        QList<FileDataWithTarget> take() { return std::move(m_fileDataList); }

    private:
        QJsonObject m_packagesObject;
        bool m_ok = true;
        QList<FileDataWithTarget> m_fileDataList;

        void parsePackagesObjects()
        {
            for (auto it = m_packagesObject.constBegin(); it != m_packagesObject.constEnd(); ++it)
                parseOsObject(it.key(), it.value().toObject());
        }

        void parseOsObject(const QString& osName, QJsonObject osObject)
        {
            for (auto it = osObject.constBegin(); it != osObject.constEnd(); ++it)
                processTargetObject(osName, it.key(), osObject);
        }

        void processTargetObject(
            const QString& osName,
            const QString& targetName,
            QJsonObject osObjet)
        {
            if (!m_ok)
                return;

            FileData fileData;
            QString fullTargetName = osName + "." + targetName;
            if (QJson::deserialize(osObjet, targetName, &fileData))
            {
                m_fileDataList.append(FileDataWithTarget(fileData, fullTargetName));
                return;
            }

            NX_ERROR(this, lm("Target json parsing failed: %1").args(fullTargetName));
            m_ok = false;
        }
    };

    QJsonObject m_topLevelObject;
    bool m_ok = true;
    UpdateData m_updateData;

    void parseVersionAndCloudHost()
    {
        if (!m_ok)
            return;

        m_updateData.version = QnSoftwareVersion(parseString(kVersionKey));
        m_updateData.cloudHost = parseString(kCloudHostKey);
    }

    QString parseString(const QString& key)
    {
        QString result;
        if (QJson::deserialize(m_topLevelObject, key, &result))
            return result;

        NX_ERROR(this, lm("%1 deserialization failed").args(key));
        m_ok = false;

        return result;
    }

    void parsePackages(const QString& key)
    {
        if (!m_ok)
            return;

        QJsonObject::const_iterator it = m_topLevelObject.find(key);
        if (it == m_topLevelObject.constEnd())
        {
            NX_ERROR(this, lm("Failed to find %1").args(key));
            m_ok = false;
            return;
        }

        PackagesParser packagesParser(key, it.value().toObject());
        if (!packagesParser.ok())
        {
            m_ok = false;
            return;
        }

        if (key == kServerPackagesKey)
        {
            fillServerPackages(packagesParser.take());
            return;
        }

        fillClientPackages(packagesParser.take());
    }

    void fillServerPackages(QList<PackagesParser::FileDataWithTarget> serverPackages)
    {
        for (const auto& fileDataWithTarget: serverPackages)
        {
            m_updateData.targetToPackage.insert(
                fileDataWithTarget.targetName,
                fileDataWithTarget.fileData);
        }
    }

    void fillClientPackages(QList<PackagesParser::FileDataWithTarget> clientPackages)
    {
        for (const auto& fileDataWithTarget: clientPackages)
        {
            m_updateData.targetToClientPackage.insert(
                fileDataWithTarget.targetName,
                fileDataWithTarget.fileData);
        }
    }
};


template<typename ParserType, typename OutData>
ResultCode applyParser(const QByteArray& rawData, OutData* outData)
{
    ParserType parser(QJsonDocument::fromJson(rawData).object());
    if (!parser.ok())
        return ResultCode::parseError;

    *outData = parser.take();
    return ResultCode::ok;
}

} // namespace

ResultCode JsonDataParser::parseMetaData(
    const QByteArray& rawData,
    UpdatesMetaData* outUpdatesMetaData)
{
    return applyParser<MetaDataParser>(rawData, outUpdatesMetaData);
}

ResultCode JsonDataParser::parseUpdateData(const QByteArray& rawData, UpdateData* outUpdateData)
{
    return applyParser<UpdateDataParser>(rawData, outUpdateData);
}

} // namespace impl
} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
