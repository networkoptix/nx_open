#include "access_scope.h"

#include <unordered_set>

#include <nx/network/http/rest/http_rest_client.h>
#include <nx/utils/std_string_utils.h>
#include <nx/utils/string.h>

#include <nx/cloud/db/client/cdb_request_path.h>

namespace {

static bool isUrl(std::string_view str)
{
    if (str.starts_with("/"))
        return true; // Support for path-only requests.

    static std::regex url_regex(
        R"((http[s]?:\/\/)?(?:www\.|(?!www))[a-zA-Z0-9][a-zA-Z0-9-]*[a-zA-Z0-9]\.[^\s]{2,}|www\.[a-zA-Z0-9][a-zA-Z0-9-]+[a-zA-Z0-9]\.[^\s]{2,}|https?:\/\/(?:www\.|(?!www))[a-zA-Z0-9]+\.[^\s]{2,}|www\.[a-zA-Z0-9]+\.[^\s]{2,})");
    return std::regex_match(str.data(), str.data() + str.size(), url_regex);
}

} // namespace

namespace nx::cloud::db::api {

nx::utils::expected<AccessScope, std::string> AccessScope::buildAccessScope(const std::string& scope)
{
    AccessScope result;
    std::optional<std::string> error;

    nx::utils::split(scope, ' ', [&result, &error](const auto& token) {
        if (isUrl(token))
        {
            result.m_urls.push_back(std::string(token));
        }
        else
        {
            const auto [tokens, count] = nx::utils::split_n<2>(token, '=');
            if (count == 2)
                result.m_attributes[std::string(tokens[0])].insert(std::string(tokens[1]));
            else
                error = std::make_optional(nx::format(
                    "Unexpected token '%1' Expected URL or KEY_VALUE_PAIR", tokens[0]).toStdString());
        }
    });

    if (error)
        return nx::utils::unexpected<std::string>(std::move(*error));

    return result;
}

AccessScope AccessScope::buildFullAccessScope(
    const std::optional<std::string>& defaultHost)
{
    AccessScope result;
    result.m_urls.resize(1);
    result.m_attributes[AccessScope::kCloudSystemId].insert(AccessScope::kAnySystemId);
    if (defaultHost)
    {
        if (defaultHost->starts_with("http"))
            result.m_urls[0] = *defaultHost;
        else
            result.m_urls[0] = "https://" + *defaultHost;
    }

    result.m_urls[0] += "/";

    return result;
}

std::string AccessScope::toString() const
{
    std::string result;
    if (!m_urls.empty())
        result += nx::utils::strJoin(m_urls, " ");

    if (!m_attributes.empty())
    {
        for (const auto& [scope, ids]: m_attributes)
            for (const auto& id: ids)
            {
                if (!result.empty())
                    result += " ";
                result += scope + "=" + id;
            }
    }
    if (!result.empty() && result[0] == ' ')
        result.erase(0, 1);

    return result;
}

AccessScope::AccessScope(std::vector<std::string> urls, Attributes attributes):
    m_urls(std::move(urls)),
    m_attributes(std::move(attributes))
{
}

bool AccessScope::isSuitableForSystem(const std::optional<std::string>& clientId) const
{
    if (!m_attributes.count(kCloudSystemId))
        return false;

    if (!clientId)
        return false;

    bool notContainUrls = m_urls.empty()
        || (m_urls.size() == 1
            && m_urls[0].ends_with(nx::network::http::rest::substituteParameters(
                kOauthTokensDeletePath, {*clientId})));

    return notContainUrls && (m_attributes.size() == 1)
        && (m_attributes.begin()->first == kCloudSystemId)
        && (m_attributes.at(kCloudSystemId).size() == 1)
        && !(*m_attributes.at(kCloudSystemId).begin() == kAnySystemId);
}

std::string AccessScope::getTheOnlySystemId() const
{
    if (m_attributes.count(kCloudSystemId) != 1)
        throw std::logic_error("the scope doesn't contain exactly one systemId");
    return *m_attributes.at(kCloudSystemId).begin();
}

const AccessScope::Attributes& AccessScope::attributes() const
{
    return m_attributes;
}

AccessScope::Attributes& AccessScope::attributes()
{
    return m_attributes;
}

bool AccessScope::isUrlAllowed(const std::string& url) const
{
    std::string_view requestPath;

    const auto sepPos = url.find('/');
    if (sepPos != std::string::npos)
        requestPath = std::string_view(url.data() + sepPos);

    auto isPrefix = [&url, &requestPath](const auto& allowedUrl)
    {
        auto allowedUrlNoScheme = cutUrlScheme(allowedUrl);
        return url.starts_with(allowedUrlNoScheme)
            // Taking into account the case where only request path is present in the scope.
            || (allowedUrlNoScheme.starts_with('/')
                && requestPath.starts_with(allowedUrlNoScheme));
    };

    return std::count_if(m_urls.cbegin(), m_urls.cend(), isPrefix);
}

static std::string_view cutScheme(std::string_view url)
{
    auto pos = url.find("://");
    if (pos != std::string_view::npos)
        url = url.substr(pos + 3);
    return url;
}

// Recognizing the following formats here:
// - example.com/some/path
// - /some/path
// Nothing else doesn't make it through here.
// @return string views referencing the original string.
static std::tuple<std::string_view, std::string_view> splitUrl(std::string_view url)
{
    if (url.starts_with('/'))
        return {std::string_view(), url}; //< url contains only path.

    auto pos = url.find('/');
    if (pos == std::string_view::npos)
        return {url, std::string_view("/")}; //< url contains only host.

    return {url.substr(0, pos), url.substr(pos)};
}

// Calculates intersection of two URLs according to the rules described for
// AccessScope::intersectScopes.
// It is expected that URLs are normalized (i.e. no scheme is present).
static std::string calcUrlsIntersection(std::string_view one, std::string_view two)
{
    const auto [oneHost, onePath] = splitUrl(one);
    const auto [twoHost, twoPath] = splitUrl(two);

    // intersecting hosts.
    std::string_view hostIntersection;
    if (oneHost.empty())
        hostIntersection = twoHost;
    else if (twoHost.empty())
        hostIntersection = oneHost;
    else if (oneHost == twoHost)
        hostIntersection = oneHost;
    else
        return std::string(); //< No intersection.

    // intersecting paths.
    std::string_view pathIntersection;
    if (onePath.size() >= twoPath.size())
    {
        if (onePath.starts_with(twoPath))
            pathIntersection = onePath;
    }
    else
    {
        if (twoPath.starts_with(onePath))
            pathIntersection = twoPath;
    }

    if (pathIntersection.empty())
        return std::string(); //< No intersection.

    return nx::utils::buildString(
        hostIntersection.empty() ? std::string_view() : std::string_view("https://"),
        hostIntersection,
        pathIntersection);
}

std::vector<std::string> AccessScope::intersectUrls(
    const std::vector<std::string>& otherUrls) const
{
    std::unordered_set<std::string> result;

    for (const auto& one: m_urls)
    {
        const auto normalizedOne = cutScheme(one);
        for (const auto& two: otherUrls)
        {
            const auto normalizedTwo = cutScheme(two);
            auto intersection = calcUrlsIntersection(normalizedOne, normalizedTwo);
            if (!intersection.empty())
                result.insert(std::move(intersection));
        }
    }

    return std::vector<std::string>(result.begin(), result.end());
}

AccessScope AccessScope::intersectScopes(const AccessScope& otherScope) const
{
    AccessScope result;

    result.m_urls = intersectUrls(otherScope.m_urls);

    auto intersectAttributes = [&result](auto& l, auto& r, const std::string& scopeName) {
        std::set_intersection(
            l.begin(),
            l.end(),
            r.begin(),
            r.end(),
            std::inserter(result.m_attributes[scopeName], result.m_attributes[scopeName].begin()));
        return result;
    };

    for (const auto& [scopeName, ids]: m_attributes)
    {
        if (otherScope.m_attributes.count(scopeName))
            intersectAttributes(
                m_attributes.at(scopeName), otherScope.m_attributes.at(scopeName), scopeName);
    }

    if (otherScope.m_attributes.count(kCloudSystemId)
        && otherScope.m_attributes.at(kCloudSystemId).count(kAnySystemId)
        && m_attributes.count(kCloudSystemId))
    {
        result.m_attributes[kCloudSystemId] = m_attributes.at(kCloudSystemId);
    }

    if (m_attributes.count(kCloudSystemId) && m_attributes.at(kCloudSystemId).count(kAnySystemId)
        && otherScope.m_attributes.count(kCloudSystemId))
    {
        result.m_attributes[kCloudSystemId] = otherScope.m_attributes.at(kCloudSystemId);
    }

    return result;
}

AccessScope AccessScope::applyAnotherScopeRestrictions(
    const std::optional<std::string>& scopeStr) const
{
    if (!scopeStr)
        return *this;

    auto scope = buildAccessScope(*scopeStr);
    if (!scope)
        return *this;

    return intersectScopes(*scope);
}

std::string AccessScope::cutUrlScheme(const std::string_view& url)
{
    static std::regex urlSchemeRegexp("http[s]?:\\/\\/");

    std::match_results<std::string_view::const_iterator> matchResult;
    if (std::regex_search(url.begin(), url.end(), matchResult, urlSchemeRegexp))
    {
        if (matchResult.size() > 0)
            return std::string(matchResult[0].second, url.end());
    }

    return std::string(url);
}

} // namespace nx::cloud::db::api
