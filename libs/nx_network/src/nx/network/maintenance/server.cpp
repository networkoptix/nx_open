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
     * Provides malloc_info(3) output which is an XML document describing the libc memory
     * allocation statistics. Refer to https://man7.org/linux/man-pages/man3/malloc_info.3.html
     * for details.
     * Info on some important metrics there:
     * - `<system type="max" size="2102161408"/>` the maximum amount of memory ever allocated by
     * the process from the OS
     * - `<system type="current" size="2070319104"/>` current amount of memory allocated by the
     * process from the OS. It includes the memory allocated by the allocator itself but not
     * currently allocated by the application
     * - `<total type="rest" count="612880" size="541640325"/>` the amount of memory currently
     * available for allocation to the application. So, this is memory allocated by the allocator,
     * but not currently used
     * <p>An important metric here WOULD BE `rest/current` which gives a portion of the memory
     * being allocated but not used. It represents the memory fragmentation level.
     *
     * %caption Get memory allocator stats
     * %ingroup Maintenance
     * %return The XML document with memory allocator statistics.
     */
    messageDispatcher->registerRequestProcessor<GetMallocInfo>(
        url::joinPath(m_maintenancePath, kMallocInfo),
        http::Method::get);

    /**%apidoc GET /placeholder/maintenance/debug/counters
     * Retrieves the list of various debug counters. These are mostly counters for various
     * allocated objects. This was found to be useful for finding memory leaks in a production
     * environment.
     * %caption Get debug counters
     * %ingroup Maintenance
     * %return The list of counters. The list of attributes here is not stable.
     */
    messageDispatcher->registerRequestProcessor<GetDebugCounters>(
        url::joinPath(m_maintenancePath, kDebugCounters),
        http::Method::get);

    /**%apidoc GET /placeholder/maintenance/version
     * Reports the component version detected at build time.
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
     * Reports the component health information. This call is mostly used by the monitoring to
     * check if the component is alive and healthy. Any 2xx response status is considered as
     * healthy. Any other status is expected to signal an issue. The message body may contain
     * additional information about the health status.
     *
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
