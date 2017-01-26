/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#include "http_request_attr_reader.h"

#include "cdb_ns.h"


namespace nx {
namespace cloud {
namespace gateway {

HttpRequestResourceReader::HttpRequestResourceReader(const nx_http::Request& request)
:
    m_request(request)
{
}

bool HttpRequestResourceReader::getAsVariant(int resID, QVariant* const value) const
{
    switch (resID)
    {
        case attr::requestPath:
            *value = m_request.requestLine.url.path();
            return true;
        default:
            return false;
    }
}

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
