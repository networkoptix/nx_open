// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

#include <transaction/transaction.h>

#include <nx/utils/log/log.h>
#include <nx/utils/string.h>
#include <nx/vms/api/types/connection_types.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/user_data.h>

namespace nx::p2p {

enum class FilterResult
{
    allow,
    deny,
    deserializedTransactionRequired,
};

struct Rule
{
    std::vector<int> ids;
    std::map<std::string, std::string> contents;
};

#define Rule_Fields (ids)(contents)

struct Filter
{
    std::string defaultAction;
    std::vector<Rule> allow;
    std::vector<Rule> deny;
};

#define Filter_Fields (defaultAction)(allow)(deny)

QN_FUSION_DECLARE_FUNCTIONS(Rule, (json))
QN_FUSION_DECLARE_FUNCTIONS(Filter, (json))

//-------------------------------------------------------------------------------------------------

class TransactionFilter
{
public:
    /**
     * @return false if the filter was not parsed.
     * NOTE: Unrecognized JSON elements are just skipped to allow future extensions.
     */
    bool parse(const QByteArray& json);

    template<typename Transaction>
    // requires std::is_base_of<QnAbstractTransaction, Transaction>
    FilterResult match(const Transaction& transaction) const
    {
        if (auto result = matchRules(transaction, m_filter.allow); result != MatchResult::notMatched)
        {
            return result == MatchResult::matched
                ? FilterResult::allow : FilterResult::deserializedTransactionRequired;
        }

        if (auto result = matchRules(transaction, m_filter.deny); result != MatchResult::notMatched)
        {
            return result == MatchResult::matched
                ? FilterResult::deny : FilterResult::deserializedTransactionRequired;
        }

        return nx::utils::stricmp(m_filter.defaultAction, "allow") == 0
            ? FilterResult::allow : FilterResult::deny;
    }

private:
    enum class MatchResult
    {
        matched,
        notMatched,
        deserializedTransactionRequired,
    };

    Filter m_filter;

    template<typename Transaction>
    // requires std::is_base_of<QnAbstractTransaction, Transaction>
    MatchResult matchRules(const Transaction& transaction, const std::vector<Rule>& rules) const
    {
        MatchResult result = MatchResult::notMatched;

        for (const auto& rule: rules)
        {
            switch (match(transaction, rule))
            {
                case MatchResult::matched:
                    return MatchResult::matched;

                case MatchResult::deserializedTransactionRequired:
                    result = MatchResult::deserializedTransactionRequired;
                    break;

                case MatchResult::notMatched:
                    break;
            }
        }

        return result;
    }

    template<typename Transaction>
    // requires std::is_base_of<QnAbstractTransaction, Transaction>
    MatchResult match(const Transaction& transaction, const Rule& rule) const
    {
        if (std::find(rule.ids.begin(), rule.ids.end(), transaction.command) == rule.ids.end())
            return MatchResult::notMatched;

        if (rule.contents.empty())
            return MatchResult::matched;

        return matchContents(transaction, rule.contents);
    }

    template<typename T>
    MatchResult matchContents(
        const ::ec2::QnTransaction<T>& transaction,
        const std::map<std::string, std::string>& contents) const
    {
        return matchSpecificContents(transaction.params, contents);
    }

    template<typename T>
    // requires std::is_same_v<T, ::ec2::QnAbstractTransaction>
    MatchResult matchContents(
        const T& /*transaction*/,
        const std::map<std::string, std::string>& /*contents*/,
        std::enable_if_t<std::is_same_v<T, ::ec2::QnAbstractTransaction>>* = nullptr) const
    {
        // Cannot do anything better since we do not have transaction data.
        return MatchResult::deserializedTransactionRequired;
    }

    MatchResult matchSpecificContents(
        const nx::vms::api::UserData& user,
        const std::map<std::string, std::string>& contents) const;

    MatchResult matchSpecificContents(
        const nx::vms::api::ResourceParamWithRefData& param,
        const std::map<std::string, std::string>& contents) const;

    template<typename T>
    MatchResult matchSpecificContents(
        const T&, const std::map<std::string, std::string>& /*contents*/) const
    {
        return MatchResult::notMatched;
    }
};

//-------------------------------------------------------------------------------------------------

// TODO: #akolesnikov This class is here temporarily. Should be removed when Cloud sends the filter text.
class CloudTransactionFilter:
    public TransactionFilter
{
public:
    CloudTransactionFilter();
};

} // namespace nx::p2p
