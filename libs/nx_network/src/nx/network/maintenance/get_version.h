// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/fusion/model_functions_fwd.h>

#include <nx/network/http/server/abstract_http_request_handler.h>

namespace nx::network::maintenance {

struct NX_NETWORK_API Version
{
    /**%apidoc major.minor.build. */
    std::string version;

    /**%apidoc Git revision of the source code. */
    std::string revision;
};

#define Version_Fields (version)(revision)

QN_FUSION_DECLARE_FUNCTIONS(Version, (json), NX_NETWORK_API)

NX_NETWORK_API Version getVersion();

//-------------------------------------------------------------------------------------------------
// GetVersion

class GetVersion:
    public http::RequestHandlerWithContext
{
protected:
    virtual void processRequest(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler) override;
};

} // namespace nx::network::maintenance