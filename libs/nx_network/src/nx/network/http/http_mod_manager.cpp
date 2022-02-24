// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_mod_manager.h"

namespace nx::network::http {

void HttpModManager::apply(Request* const request)
{
    // Applying m_urlRewriteExact.
    auto it = m_urlRewriteExact.find(request->requestLine.url.path().toStdString());
    if (it != m_urlRewriteExact.end())
        request->requestLine.url.setPath(it->second);

    // Calling custom filters.
    for (auto& mod: m_requestModifiers)
        mod(request);
}

void HttpModManager::addUrlRewriteExact(
    const std::string& originalPath,
    const std::string& effectivePath)
{
    m_urlRewriteExact.emplace(originalPath, effectivePath);
}

void HttpModManager::addCustomRequestMod(std::function<void(Request*)> requestMod)
{
    m_requestModifiers.emplace_back(std::move(requestMod));
}

} // namespace nx::network::http
