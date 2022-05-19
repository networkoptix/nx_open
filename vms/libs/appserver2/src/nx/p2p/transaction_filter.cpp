// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transaction_filter.h"

#include <core/resource/user_resource.h>
#include <nx/cloud/db/client/data/auth_data.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/string.h>
#include <nx/vms/common/system_settings.h>

namespace nx::p2p {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Rule, (json), Rule_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Filter, (json), Filter_Fields)

//-------------------------------------------------------------------------------------------------

bool TransactionFilter::parse(const QByteArray& json)
{
    return QJson::deserialize(json, &m_filter);
}

TransactionFilter::MatchResult TransactionFilter::matchSpecificContents(
    const nx::vms::api::UserData& user,
    const std::map<std::string, std::string>& contents) const
{
    if (auto it = contents.find("isCloud"); it != contents.end())
    {
        if (user.isCloud != (nx::utils::stricmp(it->second, "true") == 0))
            return MatchResult::notMatched;
    }

    return MatchResult::matched;
}

TransactionFilter::MatchResult TransactionFilter::matchSpecificContents(
    const nx::vms::api::ResourceParamWithRefData& param,
    const std::map<std::string, std::string>& contents) const
{
    if (auto it = contents.find("resourceId"); it != contents.end())
    {
        if (param.resourceId != QnUuid::fromStringSafe(it->second))
            return MatchResult::notMatched;
    }

    if (auto it = contents.find("name"); it != contents.end())
    {
        if (param.name != it->second)
            return MatchResult::notMatched;
    }

    return MatchResult::matched;
}

//-------------------------------------------------------------------------------------------------

/**
 * This filter repressents Cloud filter as of now.
 * 10200: saveSystemMergeHistoryRecord
 * 502: removeUser
 * 501: saveUser
 * 208: setResourceParam
 * 209: removeResourceParam
 * "99cbc715-539b-4bfe-856f-799b45b69b1e": admin guid
 */
static constexpr char kCloudFilterJson[] = R"json(
{
    "defaultAction": "deny",
    "allow": [
        {
            "ids": [502, 10200]
        },
        {
            "ids": [501],
            "contents": {
                "isCloud": "true"
            }
        },
        {
            "ids": [208, 209],
            "contents": {
                "resourceId": "99cbc715-539b-4bfe-856f-799b45b69b1e",
                "name": "systemName"
            }
        },
        {
            "ids": [208, 209],
            "contents": {
                "resourceId": "99cbc715-539b-4bfe-856f-799b45b69b1e",
                "name": "specificFeatures"
            }
        },
        {
            "ids": [208, 209],
            "contents": {
                "resourceId": "99cbc715-539b-4bfe-856f-799b45b69b1e",
                "name": "cloudAccountName"
            }
        },
        {
            "ids": [208, 209],
            "contents": {
                "name": "cloudUserAuthenticationInfo"
            }
        },
        {
            "ids": [208, 209],
            "contents": {
                "name": "fullUserName"
            }
        },
        {
            "ids": [208, 209],
            "contents": {
                "name": "certificate"
            }
        }
    ]
}
)json";

CloudTransactionFilter::CloudTransactionFilter()
{
    NX_CRITICAL(parse(kCloudFilterJson));
}

} // namespace nx::p2p
