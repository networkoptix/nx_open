// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <optional>
#include <string>

#include <nx/network/http/server/abstract_http_request_handler.h>
#include <nx/utils/buffer.h>
#include <nx/utils/thread/mutex.h>

namespace nx::network::http { class AbstractMessageDispatcher; }

namespace nx::network::http::test {

/**
 * Tree of some resource represented with nx::Buffer under some path.
 * The object can be registered with an HTTP server using ResourceStorage::registerHttpHandlers.
 * As a result, it will be possible to add/update/get/remove a resource using
 * HTTP POST/PUT/GET/DELETE requests.
 */
class NX_NETWORK_API ResourceStorage
{
public:
    virtual ~ResourceStorage() = default;
    void save(const std::string& path, nx::Buffer content);
    std::optional<nx::Buffer> get(const std::string& path) const;
    bool erase(const std::string& path);

    virtual void registerHttpHandlers(
        AbstractMessageDispatcher* dispatcher,
        const std::string& basePath);

protected:
    void setBasePath(const std::string& basePath);

    void postResource(
        RequestContext requestContext,
        RequestProcessedHandler handler);

    void getResource(
        RequestContext requestContext,
        RequestProcessedHandler handler);

    void deleteResource(
        RequestContext requestContext,
        RequestProcessedHandler handler);

private:
    std::map<std::string, nx::Buffer> m_pathToContent;
    std::string m_basePath;
    mutable nx::Mutex m_mutex;
};

} // namespace nx::network::http::test
