#include "async_update_checker.h"
#include <nx/utils/log/log.h>
#include "detail/data_parser/raw_data_parser_factory.h"
#include "detail/data_provider/raw_data_provider_factory.h"
#include "analytics/detected_objects_storage/analytics_events_storage_types.h"
#include "update_registry_factory.h"

namespace nx {
namespace update {
namespace info {

const QString kDefaultUrl = "http://updates.networkoptix.com";

namespace {

using namespace detail::data_parser;
using namespace detail::data_provider;
using namespace nx::utils;

class SpecificUpdatesFetcher
{
public:
    explicit SpecificUpdatesFetcher(
        const UpdatesMetaData& metaData,
        AbstractAsyncRawDataProvider* rawDataProvider,
        AbstractRawDataParser* rawDataParser,
        MoveOnlyFunc<void(ResultCode)> completionHandler)
        :
        m_metaData(metaData),
        m_rawDataProvider(rawDataProvider),
        m_rawDataParser(rawDataParser),
        m_completionHandler(std::move(completionHandler))
    {
        fetchNextCustomization();
    }

    void onSpecificDataResponse(ResultCode resultCode, QByteArray rawData)
    {
        if (resultCode == ResultCode::ok)
            return processUpdateData(std::move(rawData));

        logError("get");
        return fetchNextVersion();
    }

    detail::CustomizationVersionToUpdate take()
    {
        return std::move(m_customizationVersionToUpdate);
    }

private:
    const UpdatesMetaData& m_metaData;
    int m_customizationIndex = 0;
    int m_versionIndex = 0;
    detail::CustomizationVersionToUpdate m_customizationVersionToUpdate;
    AbstractAsyncRawDataProvider* m_rawDataProvider;
    AbstractRawDataParser* m_rawDataParser;
    MoveOnlyFunc<void(ResultCode)> m_completionHandler;

    void fetchNextCustomization()
    {
        if (m_customizationIndex >= m_metaData.customizationDataList.size())
        {
            m_completionHandler(ResultCode::ok);
            return;
        }

        fetchVersionData();
    }

    void logError(const QString& actionName) const
    {
        auto& customizationData = m_metaData.customizationDataList[m_customizationIndex];
        NX_ERROR(
            this,
            lm("Failed to %1 update data for %2 : %3").args(
                actionName,
                customizationData.name,
                customizationData.versions[m_versionIndex].toString()));
    }

    void fetchVersionData()
    {
        auto& customizationData = m_metaData.customizationDataList[m_customizationIndex];
        if (m_versionIndex >= customizationData.versions.size())
        {
            m_versionIndex = 0;
            ++m_customizationIndex;
            fetchNextCustomization();
            return;
        }

        const QString build = QString::number(customizationData.versions[m_versionIndex].build());
        m_rawDataProvider->getSpecificUpdateData(customizationData.name, build);
    }

    void fetchNextVersion()
    {
        ++m_versionIndex;
        fetchVersionData();
    }

    void processUpdateData(QByteArray rawData)
    {
        UpdateData updateData;
        const auto resultCode = m_rawDataParser->parseUpdateData(std::move(rawData), &updateData);
        if (resultCode != ResultCode::ok)
        {
            logError("parse");
            return fetchNextVersion();
        }
        const auto& customizationData = m_metaData.customizationDataList[m_customizationIndex];
        const auto& version = customizationData.versions[m_versionIndex];
        m_customizationVersionToUpdate.insert(
            detail::CustomizationVersionData(customizationData.name, version),
            std::move(updateData));

        fetchNextVersion();
    }
};

} // namespace

namespace detail {
class AsyncUpdateCheckerImpl: private detail::data_provider::AbstractAsyncRawDataProviderHandler
{
public:
    void check(const QString& baseUrl, UpdateCheckCallback callback)
    {
        m_baseUrl = baseUrl;
        m_rawDataParser = RawDataParserFactory::create();
        m_rawDataProvider = RawDataProviderFactory::create(baseUrl, this);
        m_updateCallback = std::move(callback);
        m_rawDataProvider->getUpdatesMetaInformation();
    }

private:
    QString m_baseUrl;
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
                m_rawDataParser.get(),
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
            UpdateRegistryFactory::create(
                m_baseUrl,
                std::move(m_updatesMetaData),
                m_specificUpdatesFetcher->take()));
    }
};

} // namespace detail


AsyncUpdateChecker::AsyncUpdateChecker(): m_impl(new detail::AsyncUpdateCheckerImpl)
{
}

AsyncUpdateChecker::~AsyncUpdateChecker() = default;

void AsyncUpdateChecker::check(UpdateCheckCallback callback, const QString& baseUrl)
{
    m_impl->check(baseUrl, std::move(callback));
}

QString toString(ResultCode resultCode)
{
    switch (resultCode)
    {
        case ResultCode::noData: return "No data";
        case ResultCode::getRawDataError: return "Failed to get raw response data";
        case ResultCode::ok: return "Success";
        case ResultCode::parseError: return "Failed to parse received response";
        case ResultCode::timeout: return "Timeout occured while waiting for the response";
        default:
            NX_ASSERT(false);
            return "";
    }
}
} // namespace info
} // namespace update
} // namespace nx
