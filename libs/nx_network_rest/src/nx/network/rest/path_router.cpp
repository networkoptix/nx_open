// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "path_router.h"

#include <nx/network/url/url_parse_helper.h>

#include "handler.h"

namespace nx::network::rest {

static auto pathPopFront(std::string_view path)
{
    const auto pos = path.find('/');
    return std::make_pair(
        path.substr(0, pos),
        (pos == std::string_view::npos) ? std::string_view() : path.substr(pos + 1));
}

class PathRouter::Item
{
public:
    Item() = default;
    Item(const Item&) = delete;
    Item& operator=(const Item&) = delete;
    Item(Item&&) = default;
    Item& operator=(Item&&) = default;

    void addHandler(std::string_view path, std::unique_ptr<Handler> handler);
    Result findHandler(
        const Request& request,
        const QString& pathIgnorePrefix,
        std::deque<Result>* overlaps = nullptr) const;

private:
    bool fillResultIfMatched(
        std::string_view path,
        const Request& request,
        Result* result,
        std::deque<Result>* overlaps) const;

    bool fillResultIfMatchedToFixed(
        std::string_view value,
        std::string_view path,
        const Request& request,
        Result* result,
        std::deque<Result>* overlaps) const;

    bool fillResultIfMatchedToParams(
        const std::map<std::string_view, Item>& params,
        std::string_view value,
        std::string_view path,
        const Request& request,
        Result* result,
        std::deque<Result>* overlaps) const;

    bool fillResultIfMatchedToParams(
        std::string_view value,
        std::string_view path,
        const Request& request,
        Result* result,
        std::deque<Result>* overlaps) const;

    bool fillResultIfMatchedToParams(
        std::string_view path,
        const Request& request,
        Result* result,
        std::deque<Result>* overlaps) const;

    Handler* handlerOrOptional() const
    {
        return m_optional.empty()
            ? m_handler.get()
            : m_optional.begin()->second.handlerOrOptional();
    }

private:
    std::map<std::string_view, Item> m_fixed; //< e.g. /cameras
    std::map<std::string_view, std::pair<std::regex, Item>> m_regexp; //< e.g. /^v[1-9]
    std::map<std::string_view, Item> m_param; //< e.g. /:parentId
    std::map<std::string_view, Item> m_optional; //< e.g. /:id?
    std::string_view m_restOfPathParam; //< e.g. /:path+
    std::unique_ptr<Handler> m_handler; //< end of path
};

PathRouter::PathRouter(): m_root(std::make_unique<Item>()) {}
PathRouter::~PathRouter() {}

Handler* PathRouter::findHandlerOrThrow(Request* request, const QString& pathIgnorePrefix) const
{
    auto result = m_root->findHandler(*request, pathIgnorePrefix);
    if (result.handler)
    {
        request->setConcreteIdProvided(result.handler->isConcreteIdProvidedInPath(&result));
        result.handler->modifyPathRouteResultOrThrow(&result);
        if (!result.validationPath.isEmpty())
            request->setDecodedPath(result.validationPath);
        result.handler->clarifyApiVersionOrThrow(result, request);
        request->setPathParams(std::move(result.pathParams));
        return result.handler;
    }
    return nullptr;
}

std::vector<std::pair<QString, QString>> PathRouter::findOverlaps(const Request& request) const
{
    std::deque<Result> overlaps;
    auto found = m_root->findHandler(request, /*pathIgnorePrefix*/ {}, &overlaps);
    std::vector<std::pair<QString, QString>> result;
    for (const auto& overlap: overlaps)
    {
        if (overlap.handler && overlap.handler != found.handler)
            result.push_back(std::make_pair(found.validationPath, overlap.validationPath));
    }
    return result;
}

PathRouter::Result PathRouter::Item::findHandler(
    const Request& request, const QString& pathIgnorePrefix, std::deque<Result>* overlaps) const
{
    Result result;
    auto path = url::normalizedPath(request.path(), pathIgnorePrefix).toStdString();
    fillResultIfMatched(std::move(path), request, &result, overlaps);
    return result;
}

bool PathRouter::Item::fillResultIfMatchedToParams(
    const std::map<std::string_view, Item>& params,
    std::string_view value,
    std::string_view path,
    const Request& request,
    Result* result,
    std::deque<Result>* overlaps) const
{
    for (const auto& [param, child]: params)
    {
        const int pos = result->validationPath.size();
        if (child.fillResultIfMatched(path, request, result, overlaps))
        {
            auto paramQStr = nx::toString(param);
            result->validationPath.insert(pos, QString("/{") + paramQStr + '}');
            result->pathParams.insert(paramQStr,
                QUrl::fromPercentEncoding(QByteArray(value.data(), value.size())));
            return true;
        }
    }
    return false;
}

bool PathRouter::Item::fillResultIfMatchedToParams(
    std::string_view value,
    std::string_view path,
    const Request& request,
    Result* result,
    std::deque<Result>* overlaps) const
{
    for (const auto params: {&m_param, &m_optional})
    {
        if (fillResultIfMatchedToParams(*params, value, path, request, result, overlaps))
            return true;
    }
    return false;
}

bool PathRouter::Item::fillResultIfMatchedToParams(
    std::string_view path,
    const Request& request,
    Result* result,
    std::deque<Result>* overlaps) const
{
    for (const auto params: {&m_param, &m_optional})
    {
        for (const auto& [param, child]: *params)
        {
            auto paramQStr = nx::toString(param);
            if (auto requestParam = request.param(paramQStr))
            {
                const int pos = result->validationPath.size();
                if (child.fillResultIfMatched(path, request, result, overlaps))
                {
                    result->validationPath.insert(pos, QString("/{") + paramQStr + '}');
                    result->pathParams.insert(paramQStr, *requestParam);
                    return true;
                }
            }
        }
    }
    return false;
}

bool PathRouter::Item::fillResultIfMatchedToFixed(
    std::string_view value,
    std::string_view path,
    const Request& request,
    Result* result,
    std::deque<Result>* overlaps) const
{
    if (auto fixed = m_fixed.find(value); fixed != m_fixed.end())
    {
        const int pos = result->validationPath.size();
        if (fixed->second.fillResultIfMatched(path, request, result, overlaps))
        {
            result->validationPath.insert(pos, QString('/') + nx::toString(value));
            return true;
        }
    }
    return false;
}

bool PathRouter::Item::fillResultIfMatched(
    std::string_view path,
    const Request& request,
    Result* result,
    std::deque<Result>* overlaps) const
{
    if (path.empty())
    {
        if (request.isJsonRpcRequest)
        {
            if (fillResultIfMatchedToParams(path, request, result, overlaps))
                return true;

            // There are no asterisks in JSON RPC path, simulate them.
            if (fillResultIfMatchedToParams(m_param, "*", path, request, result, overlaps))
                return true;

            if (!m_restOfPathParam.empty())
            {
                auto paramQStr = nx::toString(m_restOfPathParam);
                if (auto requestParam = request.param(paramQStr))
                {
                    result->pathParams.insert(paramQStr, *requestParam);
                    result->validationPath.append(QString("/{") + paramQStr + '}');
                }
                return result->handler = m_handler.get();
            }
        }

        result->handler = handlerOrOptional();
        return result->handler != nullptr;
    }

    bool hasMatch = false;
    const auto stop =
        [&hasMatch, &result, &overlaps]()
        {
            if (!overlaps)
                return true;

            hasMatch = true;
            overlaps->push_back(*result);
            result = &overlaps->back();
            result->handler = nullptr;
            result->validationPath.clear();
            return false;
        };

    auto [fixedOrValue, subpath] = pathPopFront(path);
    if (fillResultIfMatchedToFixed(fixedOrValue, subpath, request, result, overlaps) && stop())
        return true;

    // There are no asterisks in JSON RPC path, simulate them.
    if (request.isJsonRpcRequest && fillResultIfMatchedToFixed("*", path, request, result, overlaps))
        return true;

    for (const auto& [_, regexpWithItem]: m_regexp)
    {
        if (std::regex_match(fixedOrValue.begin(), fixedOrValue.end(), regexpWithItem.first))
        {
            const int pos = result->validationPath.size();
            if (regexpWithItem.second.fillResultIfMatched(subpath, request, result, overlaps))
            {
                result->validationPath.insert(pos, QString('/') + nx::toString(fixedOrValue));
                if (stop())
                    return true;
            }
        }
    }

    if (request.isJsonRpcRequest)
    {
        if (fillResultIfMatchedToParams(path, request, result, overlaps))
            return true;

        // There are no asterisks in JSON RPC path, simulate them.
        if (fillResultIfMatchedToParams(m_param, "*", path, request, result, overlaps))
            return true;
    }
    else
    {
        if (fillResultIfMatchedToParams(fixedOrValue, subpath, request, result, overlaps)
            && stop())
        {
            return true;
        }
    }

    if (!m_restOfPathParam.empty())
    {
        auto paramQStr = nx::toString(m_restOfPathParam);
        if (request.isJsonRpcRequest)
        {
            auto requestParam = request.param(paramQStr);
            if (!requestParam)
                return false;

            result->pathParams.insert(paramQStr, *requestParam);
        }
        else
        {
            result->pathParams.insert(paramQStr, path);
        }
        result->validationPath.append(QString("/{") + paramQStr + '}');
        result->handler = m_handler.get();
        if (stop())
            return true;
    }

    return
        (!m_optional.empty()
            && m_optional.begin()->second.fillResultIfMatched(path, request, result, overlaps))
        || hasMatch;
}

void PathRouter::addHandler(const QString& path, std::unique_ptr<Handler> handler)
{
    m_pathHolder.push_back(path.toStdString());
    m_root->addHandler(m_pathHolder.back(), std::move(handler));
}

void PathRouter::Item::addHandler(std::string_view path, std::unique_ptr<Handler> handler)
{
    if (!path.empty())
    {
        auto [item, subpath] = pathPopFront(path);
        if (item.front() == ':')
        {
            item.remove_prefix(1);
            if (item.back() == '+')
            {
                NX_CRITICAL(subpath.empty(), "path='%1', subpath='%2'", path, subpath);
                NX_CRITICAL(!m_handler.get());
                item.remove_suffix(1);
                m_restOfPathParam = std::move(item);
                m_handler = std::move(handler);
                return;
            }

            // Clang can't capture structured binding variables implicitly.
            auto add =
                [&, subpath = subpath, handler = std::move(handler)](
                    auto paramName, auto& params) mutable
                {
                    NX_CRITICAL(!subpath.empty() || params.empty(),
                        "path='%1', subpath='%2'", path, subpath);
                    params[std::move(paramName)].addHandler(subpath, std::move(handler));
                };
            if (item.back() == '?')
            {
                item.remove_suffix(1);
                add(item, m_optional);
                return;
            }

            add(item, m_param);
            return;
        }

        if (item.front() == '^')
        {
            item.remove_prefix(1);
            if (auto it = m_regexp.find(item); it != m_regexp.end())
            {
                it->second.second.addHandler(subpath, std::move(handler));
                return;
            }

            m_regexp.insert(std::make_pair(
                    std::move(item), std::make_pair(std::regex(item.begin(), item.end()), Item())))
                .first->second.second.addHandler(subpath, std::move(handler));
            return;
        }

        m_fixed[item].addHandler(subpath, std::move(handler));
        return;
    }
    NX_CRITICAL(!m_handler.get());
    m_handler = std::move(handler);
}

QString PathRouter::replaceVersionWithRegex(const std::string_view& path)
{
    constexpr std::string_view prefix = "rest/v{";
    if (!path.starts_with(prefix))
        return QString::fromLatin1(path.data(), path.size());

    const auto pathEnd = path.substr(path.find_first_of('}') + 1);
    const auto apidocVersion =
        path.substr(prefix.length(), path.length() - pathEnd.length() - prefix.length() - 1);
    const auto beginChar = apidocVersion.front();
    const auto begin = (int) (beginChar - '0');
    const auto endChar = apidocVersion.back();
    const auto end = endChar == '-' ? 9 : (int) (endChar - '0');
    return NX_FMT("rest/^v[%1-%2]%3", begin, end, pathEnd);
}

} // namespace nx::network::rest
