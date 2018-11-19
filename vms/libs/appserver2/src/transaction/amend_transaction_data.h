#pragma once

#include <core/resource_access/user_access_data.h>
#include <core/resource/param.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/full_info_data.h>

namespace ec2 {

template <typename T>
void amendOutputDataIfNeeded(const Qn::UserAccessData&, T*)
{
}

inline void amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData, nx::vms::api::ResourceParamData* paramData)
{
    if (paramData->name == Qn::CAMERA_CREDENTIALS_PARAM_NAME ||
        paramData->name == Qn::CAMERA_DEFAULT_CREDENTIALS_PARAM_NAME)
    {
        auto decryptedValue = nx::utils::decodeStringFromHexStringAES128CBC(paramData->value);
        if (accessData == Qn::kSystemAccess ||
            accessData.access == Qn::UserAccessData::Access::ReadAllResources)
            paramData->value = decryptedValue;
        else
            paramData->value = decryptedValue.left(decryptedValue.indexOf(':')) + ":******";
    }
}

inline void amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData, nx::vms::api::ResourceParamWithRefData* paramData)
{
    return amendOutputDataIfNeeded(
        accessData, static_cast<nx::vms::api::ResourceParamData*>(paramData));
}

inline void amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    std::vector<nx::vms::api::ResourceParamData>* paramDataList)
{
    for (auto& paramData : *paramDataList)
        amendOutputDataIfNeeded(accessData, &paramData);
}

inline void amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    std::vector<nx::vms::api::ResourceParamWithRefData>* paramWithRefDataList)
{
    for (auto& paramData : *paramWithRefDataList)
    {
        amendOutputDataIfNeeded(
            accessData, static_cast<nx::vms::api::ResourceParamData*>(&paramData));
    }
}

inline void amendOutputDataIfNeeded(
    const Qn::UserAccessData& accessData, nx::vms::api::FullInfoData* paramData)
{
    amendOutputDataIfNeeded(accessData, &paramData->allProperties);
}

} /* namespace ec2*/
