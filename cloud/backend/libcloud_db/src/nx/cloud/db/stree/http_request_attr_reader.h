#pragma once

#include <nx/network/http/http_types.h>
#include <nx/utils/stree/resourcecontainer.h>

namespace nx::cloud::db {

class HttpRequestResourceReader:
    public nx::utils::stree::AbstractResourceReader
{
public:
    HttpRequestResourceReader(const nx::network::http::Request& request);

    virtual bool getAsVariant(int resId, QVariant* const value) const override;

private:
    const nx::network::http::Request& m_request;
};

} // namespace nx::cloud::db
