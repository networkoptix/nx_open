#include "async_http_json_provider.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {

AsyncHttpJsonProvider::AsyncHttpJsonProvider(
    const QString& baseUrl,
    AbstractAsyncRawDataProviderHandler *handler)
    :
    m_baseUrl(baseUrl),
    m_handler(handler)
{

}

void AsyncHttpJsonProvider::getUpdatesMetaInformation()
{

}

void AsyncHttpJsonProvider::getSpecific(const QString& /*customization*/, const QString& /*version*/)
{

}

} // namespace data_provider
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
