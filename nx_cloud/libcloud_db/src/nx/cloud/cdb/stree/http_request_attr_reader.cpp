#include "http_request_attr_reader.h"

#include "cdb_ns.h"

namespace nx {
namespace cdb {

HttpRequestResourceReader::HttpRequestResourceReader(const nx::network::http::Request& request):
    m_request(request)
{
}

bool HttpRequestResourceReader::getAsVariant(int resId, QVariant* const value) const
{
    switch (resId)
    {
        case attr::requestPath:
            *value = m_request.requestLine.url.path();
            return true;
        default:
            return false;
    }
}

} // namespace cdb
} // namespace nx
