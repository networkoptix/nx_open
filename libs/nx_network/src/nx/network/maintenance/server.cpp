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

Server::Server(const std::string& basePath, std::optional<std::string> version):
    m_maintenancePath(url::joinPath(basePath, kMaintenance)),
    m_version(std::move(version))
{
}

void Server::registerRequestHandlers(
    http::server::rest::MessageDispatcher* messageDispatcher)
{
    /**%apidoc GET /placeholder/maintenance/malloc_info
     * Retrieves the statistics from the libc memory allocator. Supported on Linux only.
     * %caption Get memory allocator stats
     * %ingroup Maintenance
     * %return malloc_info(3) output. See https://man7.org/linux/man-pages/man3/malloc_info.3.html
     * for details.
     */
    messageDispatcher->registerRequestProcessor<GetMallocInfo>(
        url::joinPath(m_maintenancePath, kMallocInfo),
        http::Method::get);

    /**%apidoc GET /placeholder/maintenance/debug/counters
     * Retrieves the list of various debug counters.
     * %caption Get debug counters
     * %ingroup Maintenance
     * %return The list of counters. The result format is not stable and can be changed at any
     *     moment.
     */
    messageDispatcher->registerRequestProcessor<GetDebugCounters>(
        url::joinPath(m_maintenancePath, kDebugCounters),
        http::Method::get);

    /**%apidoc GET /placeholder/maintenance/version
     * Reports the component version.
     * %caption Get component version
     * %ingroup Maintenance
     * %return Version information.
     *     %struct Version
     */
    messageDispatcher->registerRequestProcessor(
        url::joinPath(m_maintenancePath, kVersion),
        [version = m_version](){ return std::make_unique<GetVersion>(std::move(version)); },
        http::Method::get);

    /**%apidoc GET /placeholder/maintenance/health
     * Reports the component health information.
     * %caption Get Module health
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
