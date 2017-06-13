/**********************************************************
* Sep 15, 2015
* a.kolesnikov
***********************************************************/

#include "http_request_attr_reader.h"

#include "cdb_ns.h"


namespace nx {
namespace cdb {

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

}   //cdb
}   //nx
