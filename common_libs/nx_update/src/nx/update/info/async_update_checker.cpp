#include "async_update_checker.h"
#include <nx/utils/log/log.h>
#include "detail/data_parser/raw_data_parser_factory.h"
#include "detail/data_provider/raw_data_provider_factory.h"

namespace nx {
namespace update {
namespace info {

AsyncUpdateChecker::AsyncUpdateChecker()
{
}

void AsyncUpdateChecker::check(const QString& baseUrl, UpdateCheckCallback callback)
{
    m_rawDataParser = detail::data_parser::RawDataParserFactory::create();
    m_rawDataProvider = detail::data_provider::RawDataProviderFactory::create(baseUrl, this);
    m_updateCallback = std::move(callback);

    m_rawDataProvider->getUpdatesMetaInformation();
}

void AsyncUpdateChecker::onGetUpdatesMetaInformationDone(
    ResultCode resultCode,
    QByteArray rawData)
{
    if (resultCode != ResultCode::ok)
    {
        NX_ERROR(this, lm("Error fetching data: %1").args(resultCode));
        m_updateCallback(resultCode, nullptr);
        return;
    }

    resultCode = m_rawDataParser->parseMetaData(rawData, &m_updatesMetaData);
    if (resultCode != ResultCode::ok)
    {
        NX_ERROR(this, lm("Error parsing data: %1").args(resultCode));
        m_updateCallback(resultCode, nullptr);
        return;
    }

    m_customizationIndex = 0;
    getSpecificData();
}

void AsyncUpdateChecker::getSpecificData()
{
    const QList<detail::data_parser::CustomizationData>& customizations =
        m_updatesMetaData.customizationDataList;
    if (m_customizationIndex >= customizations.size())
        return;

    const QString& customization = customizations[m_customizationIndex].name;
    const QString& version = customizations[m_customizationIndex].version;
    m_rawDataProvider->getSpecificUpdateData(customization, version);
}

void AsyncUpdateChecker::onGetSpecificUpdateInformationDone(
    ResultCode resultCode,
    QByteArray rawData)
{

}

} // namespace info
} // namespace update
} // namespace nx
