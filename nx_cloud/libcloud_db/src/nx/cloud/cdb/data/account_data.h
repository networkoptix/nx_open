#pragma once

#include <QtCore/QUrlQuery>

#include <cstdint>
#include <string>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/stree/resourcecontainer.h>
#include <nx/utils/stree/resourcenameset.h>
#include <nx/utils/uuid.h>

#include <nx/cloud/cdb/client/data/account_data.h>

namespace nx {
namespace cdb {
namespace data {

class AccountRegistrationData:
    public api::AccountRegistrationData,
    public nx::utils::stree::AbstractResourceReader
{
public:
    virtual bool getAsVariant(int resID, QVariant* const value) const override;
};

class AccountData:
    public api::AccountData,
    public nx::utils::stree::AbstractResourceReader
{
public:
    AccountData() = default;
    AccountData(AccountRegistrationData registrationData);
    AccountData(api::AccountData apiAccountData);

    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

class AccountConfirmationCode:
    public api::AccountConfirmationCode,
    public nx::utils::stree::AbstractResourceReader
{
public:
    virtual bool getAsVariant( int resID, QVariant* const value ) const override;
};

class AccountUpdateData:
    public api::AccountUpdateData,
    public nx::utils::stree::AbstractResourceReader
{
public:
    virtual bool getAsVariant(int resID, QVariant* const value) const override;
};

class AccountUpdateDataWithEmail:
    public AccountUpdateData
{
public:
    std::string email;

    AccountUpdateDataWithEmail();
    AccountUpdateDataWithEmail(AccountUpdateData&& rhs);
};

class AccountEmail:
    public api::AccountEmail,
    public nx::utils::stree::AbstractResourceReader
{
public:
    virtual bool getAsVariant(int resID, QVariant* const value) const override;
};

class TemporaryCredentialsParams:
    public api::TemporaryCredentialsParams,
    public nx::utils::stree::AbstractResourceReader,
    public nx::utils::stree::AbstractResourceWriter
{
public:
    virtual bool getAsVariant(int resID, QVariant* const value) const override;
    virtual void put(int resID, const QVariant& value) override;
};

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

class TemporaryAccountCredentials
{
public:
    /** used to find account */
    std::string accountEmail;
    /** this is actual login */
    std::string login;
    /** password to be used with \a login, not \a accountEmail! */
    std::string password;
    std::string passwordHa1;
    std::string realm;
    std::uint32_t expirationTimestampUtc;
    /** each use of credentials increases expiration time by this period. Used if non-zero */
    unsigned int prolongationPeriodSec;
    int maxUseCount;
    int useCount;
    //!this password has been sent to user's email address
    bool isEmailCode;
    AccessRestrictions accessRights;

    TemporaryAccountCredentials();
};

#define TemporaryAccountCredentials_Fields (accountEmail)(login)(passwordHa1)(realm) \
    (expirationTimestampUtc)(prolongationPeriodSec)(maxUseCount)(useCount)(isEmailCode)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (TemporaryAccountCredentials),
    (sql_record))


//#define AccountUpdateDataWithEmail_Fields (passwordHa1)(fullName)(customization)(email)
//
//QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
//    (AccountUpdateDataWithEmail),
//    (sql_record))

} // namespace data
} // namespace cdb
} // namespace nx
