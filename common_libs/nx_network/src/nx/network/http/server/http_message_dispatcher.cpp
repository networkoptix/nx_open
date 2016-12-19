#include "http_message_dispatcher.h"

namespace nx_http {

template<typename Value>
const std::pair<const QString, Value>* findByMaxPrefix(
    const std::map<QString, Value>& map, const QString& key)
{
    auto it = map.upper_bound(key);
    if (it == map.begin())
        return nullptr;

    --it;
    if (!key.startsWith(it->first))
        return nullptr;

    return &(*it);
}

void MessageDispatcher::addModRewriteRule(QString oldPrefix, QString newPrefix)
{
    QnMutexLocker lock(&m_mutex);
    NX_LOGX(lm("New rewrite rule '%1*' to '%2*'").strs(oldPrefix, newPrefix), cl_logDEBUG1);
    m_rewritePrefixes.emplace(std::move(oldPrefix), std::move(newPrefix));
}

void MessageDispatcher::applyModRewrite(QUrl* url) const
{
    QnMutexLocker lock(&m_mutex);
    if (const auto it = findByMaxPrefix(m_rewritePrefixes, url->path()))
    {
        const auto newPath = url->path().replace(it->first, it->second);
        NX_LOGX(lm("Rewride url '%1' to '%2'").strs(url->path(), newPath), cl_logDEBUG2);
        url->setPath(newPath);
    }
}

std::unique_ptr<AbstractHttpRequestHandler> MessageDispatcher::makeHandler(
    const StringType& method, const QString& path) const
{
    QnMutexLocker lock(&m_mutex);
    auto methodFactory = m_factories.find(method);
    if (methodFactory != m_factories.end())
    {
        // NOTE: Replace lines below to enable search by max path
        // if (const auto it = findByMaxPrefix(methodFactory->second, path))
        //     return it->second();

        const auto maker = methodFactory->second.find(path);
        if (maker != methodFactory->second.end())
            return maker->second();

        const auto anyMaker = methodFactory->second.find(kAnyPath);
        if (anyMaker != methodFactory->second.end())
            return anyMaker->second();
    }

    auto anyMethodFactory = m_factories.find(kAnyMethod);
    if (anyMethodFactory != m_factories.end())
    {
        // NOTE: Replace lines below to enable serach by max path
        // if (const auto it = findByMaxPrefix(anyMethodFactory->second, path))
        //     return it->second();

        const auto maker = anyMethodFactory->second.find(path);
        if (maker != anyMethodFactory->second.end())
            return maker->second();

        const auto anyMaker = anyMethodFactory->second.find(kAnyPath);
        if (anyMaker != anyMethodFactory->second.end())
            return anyMaker->second();
    }

    return std::unique_ptr<AbstractHttpRequestHandler>();
}

} // namespace nx_http
