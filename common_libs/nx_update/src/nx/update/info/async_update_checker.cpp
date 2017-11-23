#include "detail/abstract_async_raw_data_provider.h"
#include "async_update_checker.h"

namespace nx {
namespace update {
namespace info {

AsyncUpdateChecker::AsyncUpdateChecker(AbstractAsyncRawDataProviderPtr rawDataProvider):
    m_rawDataProvider(std::move(rawDataProvider))
{
}

void AsyncUpdateChecker::check(const QString& /*baseVersion*/, UpdateCheckCallback /*callback*/)
{

}

} // namespace info
} // namespace update
} // namespace nx
