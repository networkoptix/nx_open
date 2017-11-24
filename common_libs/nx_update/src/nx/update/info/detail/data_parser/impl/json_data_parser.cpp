#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <utils/common/software_version.h>

#include "json_data_parser.h"
#include "../abstract_update_meta_info.h"
#include "../abstract_target_updates_info.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {
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

struct AlternativeServerInfo
{
    QString url;
    QString name;
};
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AlternativeServerInfo, (json),
    (url)(name))


} // namespace

AbstractUpdateMetaInfo* JsonDataParser::parseMetaData(const QByteArray& rawData)
{
    QList<AlternativeServerInfo> info;
    const auto json = QJsonDocument::fromJson(rawData).object();

    if (!QJson::deserialize(json, "__info", &info))
    {
        NX_WARNING(this, "Alternative server list deserialization failed");
        return nullptr;
    }

    return nullptr;
}

AbstractTargetUpdatesInfo* JsonDataParser::parseVersionData(const QByteArray& /*rawData*/)
{
    return nullptr;
}

} // namespace impl
} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
