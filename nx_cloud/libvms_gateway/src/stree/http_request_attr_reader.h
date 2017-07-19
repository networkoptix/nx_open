/**********************************************************
* May 17, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <nx/utils/stree/resourcecontainer.h>
#include <nx/network/http/http_types.h>


namespace nx {
namespace cloud {
namespace gateway {

//TODO #ak this is a copy-paste. move to common or somewhere else. Deal with attr enum
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

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
