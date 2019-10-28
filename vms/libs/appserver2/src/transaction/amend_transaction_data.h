#pragma once

#include <core/resource_access/user_access_data.h>
#include <core/resource/param.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/full_info_data.h>

#include <vector>

class QnResourceAccessManager;

namespace ec2 {

static const QString kHiddenPasswordFiller("******");

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

} /* namespace ec2*/
