#include "async_update_checker.h"
#include "detail/data_parser/raw_data_parser_factory.h"
#include "detail/data_provider/async_raw_data_provider_factory.h"

namespace nx {
namespace update {
namespace info {

AsyncUpdateChecker::AsyncUpdateChecker()
{
}

void AsyncUpdateChecker::check(const QString& baseUrl, UpdateCheckCallback callback)
{
    m_rawDataParser = detail::data_parser::RawDataParserFactory().create();
    m_rawDataProvider = detail::data_provider::AsyncRawDataProviderFactory().create(baseUrl, this);
    m_updateCallback = std::move(callback);

    m_rawDataProvider->getUpdatesMetaInformation();
}

void AsyncUpdateChecker::onGetUpdatesMetaInformationDone(
    ResultCode /*resultCode*/,
    QByteArray /*rawData*/)
{

}

void AsyncUpdateChecker::onGetSpecificUpdateInformationDone(
    ResultCode /*resultCode*/,
    QByteArray /*rawData*/)
{

}

} // namespace info
} // namespace update
} // namespace nx
