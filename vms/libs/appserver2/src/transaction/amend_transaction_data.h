#pragma once

#include <core/resource_access/user_access_data.h>
#include <core/resource/param.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/full_info_data.h>

namespace ec2 {

// Returns true if data has been amended.
template <typename T>
bool amendOutputDataIfNeeded(const Qn::UserAccessData&, T*)
{
    return false;
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    nx::vms::api::ResourceParamData* paramData);

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    nx::vms::api::ResourceParamWithRefData* paramData);

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    std::vector<nx::vms::api::ResourceParamData>* paramDataList);

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    std::vector<nx::vms::api::ResourceParamWithRefData>* paramWithRefDataList);

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    nx::vms::api::FullInfoData* paramData);

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    nx::vms::api::CameraDataEx* paramData);

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    nx::vms::api::CameraDataExList* paramData);

} /* namespace ec2*/
