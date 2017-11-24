#pragma once

#include <QtCore>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

class AbstractCustomizationInfoList;
class AbstractTargetUpdatesInfoList;

class AbstractRawDataParser
{
public:
    virtual ~AbstractRawDataParser() {}
    virtual AbstractCustomizationInfoList* parseMetaData(const QByteArray& rawData) = 0;
    virtual AbstractTargetUpdatesInfoList* parseVersionData(const QByteArray& rawData) = 0;
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
