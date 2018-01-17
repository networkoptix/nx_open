#pragma once

#include <QtCore>
#include <nx/update/info/detail/data_parser/update_data.h>
#include <nx/update/info/detail/data_parser/updates_meta_data.h>
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
    virtual ResultCode parseMetaData(const QByteArray& rawData, UpdatesMetaData* outUpdatesMetaData) = 0;
    virtual ResultCode parseUpdateData(const QByteArray& rawData, UpdateData* outUpdateData) = 0;
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
