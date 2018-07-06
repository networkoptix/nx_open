#include "http_mod_manager.h"

namespace nx {
namespace network {
namespace http {

HttpModManager::HttpModManager(QObject* parent /*= nullptr*/):
    base_type(parent)
{
}

void HttpModManager::apply(Request* const request)
{
    // Applying m_urlRewriteExact.
    auto it = m_urlRewriteExact.find(request->requestLine.url.path());
    if (it != m_urlRewriteExact.end())
        request->requestLine.url.setPath(it->second);

    // Calling custom filters.
    for (auto& mod: m_requestModifiers)
        mod(request);
}

void HttpModManager::addUrlRewriteExact(
    const QString& originalPath,
    const QString& effectivePath)
{
    m_urlRewriteExact.emplace(originalPath, effectivePath);
}

void HttpModManager::addCustomRequestMod(std::function<void(Request*)> requestMod)
{
    m_requestModifiers.emplace_back(std::move(requestMod));
}

} // namespace nx
} // namespace network
} // namespace http
