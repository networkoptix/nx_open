#pragma once

#include <nx/network/http/http_types.h>
#include <nx/utils/stree/resourcecontainer.h>

namespace nx {
namespace cdb {

class HttpRequestResourceReader:
    public nx::utils::stree::AbstractResourceReader
{
public:
    HttpRequestResourceReader(const nx_http::Request& request);

    virtual bool getAsVariant(int resId, QVariant* const value) const override;

private:
    const nx_http::Request& m_request;
};

} // namespace cdb
} // namespace nx
