#pragma once

#include <QtCore>
#include <nx/update/info/detail/data_parser/abstract_raw_data_parser.h>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

class AbstractCustomizationInfoList;
class AbstractTargetUpdatesInfoList;

class JsonDataParser: public AbstractRawDataParser
{
public:
    virtual AbstractCustomizationInfoList* parseMetaData(const QByteArray& rawData) override;
    virtual AbstractTargetUpdatesInfoList* parseVersionData(const QByteArray& rawData) override;
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
