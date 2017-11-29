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
    virtual void onGetUpdatesMetaInformationDone(ResultCode resultCode, QByteArray rawData) = 0;
    virtual void onGetSpecificUpdateInformationDone(ResultCode resultCode, QByteArray rawData) = 0;
};

} // namespace data_provider
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
