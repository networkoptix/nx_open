#pragma once

#include <nx/utils/stree/resourcecontainer.h>
#include <nx/utils/stree/resourcenameset.h>

namespace nx::cloud::db {
namespace data {

class FieldRule
{
public:
    int dataFieldId;
    bool required;

    FieldRule() = delete;

    std::string toString(const nx::utils::stree::ResourceNameSet& nameset) const;
};

class ApiRequestRule
{
public:
    std::string path;

    ApiRequestRule() = default;
    ApiRequestRule(const std::string& path);
    ApiRequestRule(const char* path);
    ApiRequestRule(
        const std::string& path,
        const std::vector<FieldRule>& fieldRules = {});

    /**
     * See AccessRestrictions::toString(), api_method_rule.
     */
    std::string toString(const nx::utils::stree::ResourceNameSet& nameset) const;
    bool parse(
        const nx::utils::stree::ResourceNameSet& nameset,
        const std::string& str);

    bool match(const nx::utils::stree::AbstractResourceReader& requestAttributes) const;

private:
    std::vector<FieldRule> m_fieldRules;

    bool parseFieldRules(
        const nx::utils::stree::ResourceNameSet& nameset,
        const std::string& str);
    std::string fieldRulesToString(
        const nx::utils::stree::ResourceNameSet& nameset) const;
};

class RequestRuleGroup
{
public:
    std::vector<ApiRequestRule> rules;

    bool empty() const;
    bool match(
        const std::string& path,
        const nx::utils::stree::AbstractResourceReader& requestAttributes) const;
    /**
     * See AccessRestrictions::toString() for format description.
     */
    std::string toString(
        const nx::utils::stree::ResourceNameSet& nameset,
        const std::string& rulePrefix) const;

    void add(const ApiRequestRule& rule);
};

class AccessRestrictions
{
public:
    /** exact names of allowed requests. if empty, \a requestsDenied is analyzed */
    RequestRuleGroup requestsAllowed;
    RequestRuleGroup requestsDenied;

    /** ABNF syntax for serialized format:
        auth_rights = *(api_method_rule ":")
        api_method_rule = control_modifier api_method_name [; field_rules]
        control_modifier = "+" | "-"
        api_method_name = uri_abs_path
        field_rules = field_rule [, field_rule]
        field_rule = control_modifier field_name
        field_name = TEXT
    */
    std::string toString(const nx::utils::stree::ResourceNameSet& nameset) const;
    bool parse(
        const nx::utils::stree::ResourceNameSet& nameset,
        const std::string& str);

    bool authorize(const nx::utils::stree::AbstractResourceReader& requestAttributes) const;
};

} // namespace data
} // namespace nx::cloud::db
