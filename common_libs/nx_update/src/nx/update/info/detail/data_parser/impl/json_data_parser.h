#pragma once

#include <memory>
#include <QtCore>
#include <nx/update/info/detail/data_parser/abstract_raw_data_parser.h>
#include <nx/update/info/result_code.h>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {
namespace impl {

class JsonDataParser: public AbstractRawDataParser
{
public:
    virtual ResultCode parseMetaData(
        const QByteArray& rawData,
        UpdatesMetaData* outUpdatesMetaData) override;
    virtual ResultCode parseUpdateData(
        const QByteArray& rawData,
        UpdateData* outUpdateData) override;
};

} // namespace impl
} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
