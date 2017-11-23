#pragma once

#include <QtCore>
#include <nx/update/info/result_code.h>

namespace nx {
namespace update {
namespace info {
namespace detail {


class AbstractCustomizationInfoList;

class AbstractAsyncRawDataProviderHandler
{
public:
    virtual ~AbstractAsyncRawDataProviderHandler() {}
    virtual onUpdatesMetaInformationDone(
        ResultCode resultCode,
        AbstractCustomizationInfoList* customizationInfoList) = 0;
    virtual onSpecificDone(ResultCode resultCode, const QByteArray& rawData) = 0;
};

} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
