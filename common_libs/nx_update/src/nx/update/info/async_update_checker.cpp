#include "async_update_checker.h"
#include <nx/utils/log/log.h>
#include "detail/data_parser/raw_data_parser_factory.h"
#include "detail/data_provider/raw_data_provider_factory.h"
#include "analytics/detected_objects_storage/analytics_events_storage_types.h"
#include "impl/update_registry_factory.h"

namespace nx {
namespace update {
namespace info {

namespace {

using namespace detail::data_parser;
using namespace detail::data_provider;
using namespace nx::utils;

using UpdateDataList = QList<UpdateData>;

class SpecificUpdatesFetcher
{
public:
    explicit SpecificUpdatesFetcher(
        const UpdatesMetaData& metaData,
        AbstractAsyncRawDataProvider* rawDataProvider,
        MoveOnlyFunc<void(ResultCode)> completionHandler)
        :
        m_metaData(metaData),
        m_rawDataProvider(rawDataProvider),
        m_completionHandler(std::move(completionHandler))
    {
        fetchNextCustomization();
    }

    void onSpecificDataResponse(ResultCode resultCode, QByteArray rawData)
    {
        if (resultCode != ResultCode::ok)
        {
            auto& customizationData = m_metaData.customizationDataList[m_customizationIndex];
            QString errorString =
                lm("Failed to get update data for %1 : %2")
                    .args(
                        customizationData.name,
                        customizationData.versions[m_versionIndex].toString());

            NX_ERROR(this, errorString);
            m_completionHandler(resultCode);
            return;
        }

        ++m_versionIndex;
        fetchNextVersion();
    }

    UpdateDataList take() { return std::move(m_updates); }

private:
    const UpdatesMetaData& m_metaData;
    int m_customizationIndex = 0;
    int m_versionIndex = 0;
    UpdateDataList m_updates;
    AbstractAsyncRawDataProvider* m_rawDataProvider;
    MoveOnlyFunc<void(ResultCode)> m_completionHandler;

    void fetchNextCustomization()
    {
        if (m_customizationIndex >= m_metaData.customizationDataList.size())
        {
            m_completionHandler(ResultCode::ok);
            return;
        }

        fetchNextVersion();
    }

    void fetchNextVersion()
    {
        auto& customizationData = m_metaData.customizationDataList[m_customizationIndex];
        if (m_versionIndex >= customizationData.versions.size())
        {
            m_versionIndex = 0;
            ++m_customizationIndex;
            fetchNextCustomization();
            return;
        }

        QString version = customizationData.versions[m_versionIndex].toString();
        m_rawDataProvider->getSpecificUpdateData(customizationData.name, version);
    }
};

} // namespace

namespace detail {
class AsyncUpdateCheckerImpl: private detail::data_provider::AbstractAsyncRawDataProviderHandler
{
public:
    void check(const QString& baseUrl, UpdateCheckCallback callback)
    {
        m_rawDataParser = RawDataParserFactory::create();
        m_rawDataProvider = RawDataProviderFactory::create(baseUrl, this);
        m_updateCallback = std::move(callback);
        m_rawDataProvider->getUpdatesMetaInformation();
    }

private:
    AbstractAsyncRawDataProviderPtr m_rawDataProvider;
    AbstractRawDataParserPtr m_rawDataParser;
    UpdateCheckCallback m_updateCallback;
    UpdatesMetaData m_updatesMetaData;
    std::unique_ptr<SpecificUpdatesFetcher> m_specificUpdatesFetcher;

    void onGetUpdatesMetaInformationDone(ResultCode resultCode, QByteArray rawData) override
    {
        if (resultCode != ResultCode::ok)
        {
            NX_ERROR(this, lm("Error fetching data: %1").args((int) resultCode));
            m_updateCallback(resultCode, nullptr);
            return;
        }

        resultCode = m_rawDataParser->parseMetaData(std::move(rawData), &m_updatesMetaData);
        if (resultCode != ResultCode::ok)
        {
            NX_ERROR(this, lm("Error parsing data: %1").args((int) resultCode));
            m_updateCallback(resultCode, nullptr);
            return;
        }

        using namespace std::placeholders;
        m_specificUpdatesFetcher.reset(
            new SpecificUpdatesFetcher(
                m_updatesMetaData,
                m_rawDataProvider.get(),
                std::bind(&AsyncUpdateCheckerImpl::onSpecificUpdateFetcherDone, this, _1)));
    }

    void onGetSpecificUpdateInformationDone(ResultCode resultCode, QByteArray rawData) override
    {
        m_specificUpdatesFetcher->onSpecificDataResponse(resultCode, std::move(rawData));
    }

    void onSpecificUpdateFetcherDone(ResultCode resultCode)
    {
        if (resultCode != ResultCode::ok)
        {
            m_updateCallback(resultCode, nullptr);
            return;
        }

        m_updateCallback(
            resultCode,
            impl::UpdateRegistryFactory::create(
                std::move(m_updatesMetaData),
                m_specificUpdatesFetcher->take()));
    }
};

} // namespace detail


AsyncUpdateChecker::AsyncUpdateChecker(): m_impl(new detail::AsyncUpdateCheckerImpl)
{
}

AsyncUpdateChecker::~AsyncUpdateChecker()
{
}

void AsyncUpdateChecker::check(const QString& baseUrl, UpdateCheckCallback callback)
{
    m_impl->check(baseUrl, std::move(callback));
}

} // namespace info
} // namespace update
} // namespace nx
