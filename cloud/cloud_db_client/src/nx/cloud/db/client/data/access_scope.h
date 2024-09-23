// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <nx/utils/std/expected.h>

namespace nx::cloud::db::api {

/**
 * Access scope. It consists of a list of URLs and a set of attributes.
 * The following URLs formats are recognized:
 * - fully qualified URL (with scheme, host, and path): E.g., https://example.com/some/path
 * - URL without scheme: E.g., example.com/some/path. Note: this is not a valid URL,
 * but it is supported for backward compatibility
 * - path only (URL without scheme and host). E.g., /some/path
 */
class AccessScope
{
public:
    using Attributes = std::unordered_map<std::string, std::set<std::string>>;

    static constexpr char kCloudSystemId[] = "cloudSystemId";
    static constexpr char kAnySystemId[] = "*";

    AccessScope() = default;
    AccessScope(std::vector<std::string> urls, Attributes attributes);

    /**
     * @return true if the scope is suitable to be used by a system. I.e., the scope only allows
     * access to a system. I.e., the scope only contains "cloudSystemId" attribute.
     */
    bool isSuitableForSystem(const std::optional<std::string>& clientId) const;

    std::string getTheOnlySystemId() const;

    /**
     * @param url in format <host>/<path>. No scheme must be present!
     * @return true if the scope has an URL which is a prefix of the given url. false otherwise.
     */
    bool isUrlAllowed(const std::string& url) const;

    std::string toString() const;

    const std::vector<std::string>& urls() const { return m_urls; }
    std::vector<std::string>& urls() { return m_urls; }

    const Attributes& attributes() const;
    Attributes& attributes();

    std::vector<std::string> intersectUrls(const std::vector<std::string>& otherUrls) const;

    /**
     * Calculates intersection of two scopes.
     * URLs intersection is calculated as follows:
     * - all URLs are considered to be path to some resources (like in REST)
     * - so, an intersection of two URLs is a URL that is a subset of both URLs. E.g.,
     *   - intersection of path /some/path and /some will be /some/path
     *   - intersection of https://example.com/ and /some/path will be https://example.com/some/path.
     *     Note that /some/path is considered to be valid for any host.
     *
     * Intersection of attributes is a subset of name=value pairs that is prsent in both scopes.
     */
    AccessScope intersectScopes(const AccessScope& otherScope) const;

    AccessScope applyAnotherScopeRestrictions(const std::optional<std::string>& scope) const;

    /**
     * @param scope valid scope.
     * SCOPE = *URL [ SP KEY_VALUE_PAIRS ]
     * KEY_VALUE_PAIRS = KEY "=" VALUE *( SP KEY "=" VALUE )
     * KEY = "cloudSystemId" | text
     * VALUE = "*" | text
     */
    static nx::utils::expected<AccessScope, std::string> buildAccessScope(const std::string& scope);

    /**
     * Builds scope "cloudSystemId=*" [ SP https://defaultHost/ ]
     */
    static AccessScope buildFullAccessScope(
        const std::optional<std::string>& defaultHost);

private:
    static std::string cutUrlScheme(const std::string_view& url);

private:
    std::vector<std::string> m_urls;
    Attributes m_attributes;
};

} // namespace nx::cloud::db::api
