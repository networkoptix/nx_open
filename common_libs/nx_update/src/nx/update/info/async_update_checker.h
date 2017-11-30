#pragma once

#include <memory>
#include <nx/update/info/detail/fwd.h>
#include <nx/update/info/detail/data_provider/abstract_async_raw_data_provider_handler.h>
#include <nx/update/info/abstract_update_registry.h>

namespace nx {
namespace update {
namespace info {

using AbstractUpdateRegistryPtr = std::unique_ptr<AbstractUpdateRegistry>;

using UpdateCheckCallback = utils::MoveOnlyFunc<void(ResultCode, AbstractUpdateRegistryPtr)>;

class AsyncUpdateChecker: private detail::data_provider::AbstractAsyncRawDataProviderHandler
{
public:
    AsyncUpdateChecker();
    void check(const QString& baseUrl, UpdateCheckCallback callback);

private:
    detail::AbstractAsyncRawDataProviderPtr m_rawDataProvider;
    detail::AbstractRawDataParserPtr m_rawDataParser;
    UpdateCheckCallback m_updateCallback;
    detail::data_parser::UpdatesMetaData m_updatesMetaData;
    int m_customizationIndex;

    virtual void onGetUpdatesMetaInformationDone(
        ResultCode resultCode,
        QByteArray rawData) override;
    virtual void onGetSpecificUpdateInformationDone(
        ResultCode resultCode,
        QByteArray rawData) override;

    void getSpecificData();
};

} // namespace info
} // namespace update
} // namespace nx
