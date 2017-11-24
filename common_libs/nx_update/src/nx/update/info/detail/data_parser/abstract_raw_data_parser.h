#pragma once

#include <QtCore>
#include <nx/update/info/detail/data_parser/update_meta_information.h>
#include <nx/update/info/detail/data_parser/target_updates_information.h>
#include <nx/update/info/result_code.h>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

class AbstractRawDataParser
{
public:
    virtual ~AbstractRawDataParser() {}
    virtual ResultCode parseMetaData(
        const QByteArray& rawData,
        UpdateMetaInformation* outUpdateMetaInformation) = 0;
    virtual TargetUpdatesInformation parseVersionData(
        const QByteArray& rawData,
        TargetUpdatesInformation* outTargetUpdatesInformation) = 0;
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
