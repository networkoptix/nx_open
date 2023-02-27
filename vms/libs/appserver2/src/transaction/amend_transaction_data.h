// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_property_key.h>
#include <core/resource_access/user_access_data.h>
#include <nx/vms/api/data/full_info_data.h>
#include <nx/vms/api/data/resource_data.h>
#include <transaction/transaction.h>

#include <vector>

class QnResourceAccessManager;

namespace nx::vms::api { struct CameraDataEx; }

namespace ec2 {

extern const QString kHiddenPasswordFiller;
extern const std::set<QString> kResourceParamToAmend;

// Returns true if data has been amended.
template <typename T>
bool amendOutputDataIfNeeded(const Qn::UserAccessData&, QnResourceAccessManager*, T*)
{
    return false;
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ResourceParamData* paramData);

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ResourceParamWithRefData* paramData);

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::FullInfoData* paramData);

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::CameraDataEx* paramData);

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::EventRuleData* rule);

bool amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::StorageData* storageData);

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::UserData* userData);

bool amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::MediaServerData* mediaServerData);

bool amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::MediaServerDataEx* mediaServerDataEx);

bool amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ServerFootageData* serverFootageData);

template<typename T>
bool amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    std::vector<T>* dataList)
{
    bool result = false;
    for (auto& entry: *dataList)
        result |= amendOutputDataIfNeeded(accessData, accessManager, &entry);

    return result;
}

} // namespace ec2
