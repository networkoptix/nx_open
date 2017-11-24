#include "json_data_parser.h"
#include "abstract_customization_info_list.h"
#include "abstract_target_updates_info_list.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

AbstractCustomizationInfoList* JsonDataParser::parseMetaData(const QByteArray& /*rawData*/)
{
    return nullptr;
}

AbstractTargetUpdatesInfoList* JsonDataParser::parseVersionData(const QByteArray& /*rawData*/)
{
    return nullptr;
}

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
