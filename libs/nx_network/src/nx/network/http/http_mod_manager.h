// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <list>
#include <map>

#include <nx/utils/singleton.h>

#include "http_types.h"

namespace nx::network::http {

/**
 * Reponsible for modifying HTTP request.
 */
class NX_NETWORK_API HttpModManager
{
public:
    /**
     * Performs some modifications on request.
     */
    void apply(Request* const request);

    /**
     * Replaces path originalPath to effectivePath.
     */
    void addUrlRewriteExact(const std::string& originalPath, const std::string& effectivePath);
    /**
     * Register functor that will receive every incoming request
     * before any processing and have a chance to modify it.
     */
    void addCustomRequestMod(std::function<void(Request*)> requestMod);

private:
    std::map<std::string, std::string> m_urlRewriteExact;
    std::list<std::function<void(Request*)>> m_requestModifiers;
};

} // namespace nx::network::http
