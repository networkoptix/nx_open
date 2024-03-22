// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <core/resource/resource_property_key.h>
#include <nx/network/rest/user_access_data.h>
#include <nx/vms/api/data/full_info_data.h>
#include <nx/vms/api/data/resource_data.h>
#include <transaction/transaction.h>

class QnResourceAccessManager;

namespace nx::vms::api { struct CameraDataEx; }

namespace ec2 {

extern const std::set<QString> kResourceParamToAmend;

// Returns true if data has been amended.
template <typename T>
bool amendOutputDataIfNeeded(const nx::network::rest::UserAccessData&, QnResourceAccessManager*, T*)
{
    return false;
}

bool amendOutputDataIfNeeded(const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::UserData* userData);

bool amendOutputDataIfNeeded(const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ResourceParamData* paramData);

bool amendOutputDataIfNeeded(const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ResourceParamWithRefData* paramData);

bool amendOutputDataIfNeeded(const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::FullInfoData* paramData);

bool amendOutputDataIfNeeded(const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::CameraDataEx* paramData);

bool amendOutputDataIfNeeded(const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::EventRuleData* rule);

bool amendOutputDataIfNeeded(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::StorageData* storageData);

bool amendOutputDataIfNeeded(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::MediaServerData* mediaServerData);

bool amendOutputDataIfNeeded(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::MediaServerDataEx* mediaServerDataEx);

bool amendOutputDataIfNeeded(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ServerFootageData* serverFootageData);

template<typename T>
bool amendOutputDataIfNeeded(
    const nx::network::rest::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    std::vector<T>* dataList)
{
    bool result = false;
    for (auto& entry: *dataList)
        result |= amendOutputDataIfNeeded(accessData, accessManager, &entry);

    return result;
}

} // namespace ec2
