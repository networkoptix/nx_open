// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/reflect/instrument.h>

namespace nx::network::maintenance {

struct NX_NETWORK_API Version
{
    /**%apidoc major.minor.build. */
    std::string version;

    /**%apidoc Git revision of the source code. */
    std::string revision;
};

NX_REFLECTION_INSTRUMENT(Version, (version)(revision))

// Return the VMS version as reported nx::build_info library.
NX_NETWORK_API Version getVersion();

//-------------------------------------------------------------------------------------------------
// GetVersion

/**
 * An HTTP handler that returns the VMS version by default. It can be overriden to return a
 * different version, e.g. the Cloud version, if the server is running on a cloud service.
 */
class GetVersion:
    public http::RequestHandlerWithContext
{
public:
    GetVersion(std::optional<std::string> version = std::nullopt);

protected:
    virtual void processRequest(
        http::RequestContext requestContext,
        http::RequestProcessedHandler completionHandler) override;

private:
    std::optional<std::string> m_version;
};

} // namespace nx::network::maintenance
