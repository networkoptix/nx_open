#pragma once

#include <nx/update/info/detail/data_provider/abstract_async_raw_data_provider.h>
#include <nx/update/info/detail/data_provider/abstract_async_raw_data_provider_handler.h>
#include "nx/update/info/detail/data_provider/test_support/json_data.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {
namespace test_support {

class SingleThreadExecutor;

class NX_UPDATE_API AsyncJsonProviderMockup: public AbstractAsyncRawDataProvider
{
public:
    AsyncJsonProviderMockup(data_provider::AbstractAsyncRawDataProviderHandler* handler);
    ~AsyncJsonProviderMockup();
    virtual void getUpdatesMetaInformation() override;
    virtual void getSpecificUpdateData(
        const QString& customization,
        const QString& version) override;

private:
    AbstractAsyncRawDataProviderHandler* m_handler;
    std::unique_ptr<SingleThreadExecutor> m_executor;
};

} // namespace test_support
} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
