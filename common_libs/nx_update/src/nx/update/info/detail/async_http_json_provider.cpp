#include "async_http_json_provider.h"

namespace nx {
namespace update {
namespace info {
namespace detail {

AsyncHttpJsonProvider::AsyncHttpJsonProvider(const QString& baseUrl):
    m_baseUrl(baseUrl)
{

}

void AsyncHttpJsonProvider::getUpdatesMetaInformation(AbstractAsyncRawDataProviderHandler* /*handler*/)
{

}

void AsyncHttpJsonProvider::getSpecific(
    AbstractAsyncRawDataProviderHandler* /*handler*/,
    const QString& /*customization*/,
    const QString& /*version*/)
{

}

} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
