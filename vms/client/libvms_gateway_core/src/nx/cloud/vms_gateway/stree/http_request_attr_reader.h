// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

//TODO #akolesnikov this is a copy-paste. move to common or somewhere else. Deal with attr enum
class HttpRequestResourceReader
:
    public nx::utils::stree::AbstractResourceReader
{
public:
    HttpRequestResourceReader(const nx::network::http::Request& request);

    //!Implementation of \a AbstractResourceReader::getAsVariant
    virtual bool getAsVariant(int resID, QVariant* const value) const override;

private:
    const nx::network::http::Request& m_request;
};

}   //namespace cloud
}   //namespace gateway
}   //namespace nx
