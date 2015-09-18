/**********************************************************
* Sep 15, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_STREE_HTTP_REQUEST_ATTR_READER_H
#define NX_CDB_STREE_HTTP_REQUEST_ATTR_READER_H

#include <plugins/videodecoder/stree/resourcecontainer.h>
#include <utils/network/http/httptypes.h>


namespace nx {
namespace cdb {

class HttpRequestResourceReader
:
    public stree::AbstractResourceReader
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
