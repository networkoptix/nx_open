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
        return;
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
private:
};

} // namespace

ResultCode JsonDataParser::parseMetaData(
    const QByteArray& rawData,
    UpdatesMetaData* outUpdatesMetaData)
{
    MetaDataParser metaDataParser(QJsonDocument::fromJson(rawData).object());
    if (!metaDataParser.ok())
        return ResultCode::parseError;

    *outUpdatesMetaData = metaDataParser.take();

    return ResultCode::ok;
}

ResultCode JsonDataParser::parseUpdateData(
    const QByteArray& /*rawData*/,
    UpdateData* /*outUpdateData*/)
{
    return ResultCode::parseError;
}

} // namespace impl
} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
