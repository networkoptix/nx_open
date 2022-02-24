// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server.h"

#include <nx/build_info.h>
#include <nx/network/http/http_types.h>
#include <nx/network/url/url_parse_helper.h>

#include "get_debug_counters.h"
#include "get_health.h"
#include "get_malloc_info.h"
#include "get_version.h"
#include "request_path.h"

namespace nx::network::maintenance {

Server::Server(const std::string& basePath):
    m_maintenancePath(url::joinPath(basePath, kMaintenance))
{
}

void Server::registerRequestHandlers(
    http::server::rest::MessageDispatcher* messageDispatcher)
{
    /**%apidoc GET /placeholder/maintenance/malloc_info
     * %caption Get statistics from the libc allocator. Supported on Unix only.
     * %ingroup Maintenance
     * %return malloc_info(3) output. See https://man7.org/linux/man-pages/man3/malloc_info.3.html
     * for details.
     */
    messageDispatcher->registerRequestProcessor<GetMallocInfo>(
        url::joinPath(m_maintenancePath, kMallocInfo),
        http::Method::get);

    /**%apidoc GET /placeholder/maintenance/debug/counters
     * This result format is not stable and can be changed at any moment.
     * %caption Get list of various debug counters.
     * %ingroup Maintenance
     * %return The list of counters.
     */
    messageDispatcher->registerRequestProcessor<GetDebugCounters>(
        url::joinPath(m_maintenancePath, kDebugCounters),
        http::Method::get);

    /**%apidoc GET /placeholder/maintenance/version
     * %caption Get version of the module.
     * %ingroup Maintenance
     * %return Version information.
     *     %struct Version
     */
    messageDispatcher->registerRequestProcessor<GetVersion>(
        url::joinPath(m_maintenancePath, kVersion),
        http::Method::get);

    /**%apidoc GET /placeholder/maintenance/health
     * %caption Get module health information.
     * %ingroup Maintenance
     * %return Application-specific health information.
     */
    messageDispatcher->registerRequestProcessor<GetHealth>(
        url::joinPath(m_maintenancePath, kHealth),
        http::Method::get);

    m_logServer.registerRequestHandlers(
        url::joinPath(m_maintenancePath, kLog),
        messageDispatcher);

    m_statisticsServer.registerRequestHandlers(
        url::joinPath(m_maintenancePath, kStatistics),
        messageDispatcher);
}

std::string Server::maintenancePath() const
{
    return m_maintenancePath;
}

} // namespace nx::network::maintenance
