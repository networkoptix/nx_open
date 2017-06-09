/**********************************************************
* Sep 15, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_STREE_HTTP_REQUEST_ATTR_READER_H
#define NX_CDB_STREE_HTTP_REQUEST_ATTR_READER_H

#include <nx/utils/stree/resourcecontainer.h>
#include <nx/network/http/http_types.h>


namespace nx {
namespace cdb {

class HttpRequestResourceReader
:
    public nx::utils::stree::AbstractResourceReader
{
public:
    HttpRequestResourceReader(const nx_http::Request& request);

    //!Implementation of \a AbstractResourceReader::getAsVariant
    virtual bool getAsVariant(int resID, QVariant* const value) const override;

private:
    const nx_http::Request& m_request;
};

}   //cdb
}   //nx

#endif  //NX_CDB_STREE_HTTP_REQUEST_ATTR_READER_H
