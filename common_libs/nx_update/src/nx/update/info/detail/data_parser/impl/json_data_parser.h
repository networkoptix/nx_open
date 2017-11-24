#pragma once

#include <memory>
#include <QtCore>
#include <nx/update/info/detail/data_parser/abstract_raw_data_parser.h>

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
        UpdateMetaInformation* outUpdateMetaInformation) override;
    virtual TargetUpdatesInformation parseVersionData(
        const QByteArray& rawData,
        TargetUpdatesInformation* outTargetUpdatesInformation) override;
};

} // namespace impl
} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
