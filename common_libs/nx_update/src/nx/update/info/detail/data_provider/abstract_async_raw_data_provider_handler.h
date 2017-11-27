#pragma once

#include <QtCore>
#include <nx/update/info/result_code.h>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {

class AbstractCustomizationInfoList;

class AbstractAsyncRawDataProviderHandler
{
public:
    virtual ~AbstractAsyncRawDataProviderHandler() {}
    virtual onUpdatesMetaInformationDone(ResultCode resultCode, const QByteArray& rawData) = 0;
    virtual onSpecificDone(ResultCode resultCode, const QByteArray& rawData) = 0;
};

} // namespace data_provider
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
