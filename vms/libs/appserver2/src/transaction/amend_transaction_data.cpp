#include "amend_transaction_data.h"

#include <utils/crypt/symmetrical.h>
#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/discovery_data.h>
#include <nx/vms/api/data/analytics_data.h>
#include <nx/vms/api/data/full_info_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/user_role_data.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/access_rights_data.h>
#include <nx/vms/api/data/videowall_data.h>
#include <nx/vms/api/data/layout_tour_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/webpage_data.h>
#include <nx/vms/api/data/resource_type_data.h>
#include <nx/vms/api/data/camera_history_data.h>
#include <nx/vms/api/data/event_rule_data.h>

namespace ec2 {

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    nx::vms::api::ResourceParamData* paramData)
{
    if (paramData->name == ResourcePropertyKey::kCredentials ||
        paramData->name == ResourcePropertyKey::kDefaultCredentials)
    {
        auto decryptedValue = nx::utils::decodeStringFromHexStringAES128CBC(paramData->value);
        if (accessData == Qn::kSystemAccess ||
            accessData.access == Qn::UserAccessData::Access::ReadAllResources)
            paramData->value = decryptedValue;
        else
            paramData->value = decryptedValue.left(decryptedValue.indexOf(':')) + ":******";

        return true;
    }

    return false;
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    nx::vms::api::ResourceParamWithRefData* paramData)
{
    return amendOutputDataIfNeeded(
        accessData, static_cast<nx::vms::api::ResourceParamData*>(paramData));
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    std::vector<nx::vms::api::ResourceParamData>* paramDataList)
{
    bool result = false;
    for (auto& paramData : *paramDataList)
        result |= amendOutputDataIfNeeded(accessData, &paramData);

    return result;
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    std::vector<nx::vms::api::ResourceParamWithRefData>* paramWithRefDataList)
{
    bool result = false;
    for (auto& paramData : *paramWithRefDataList)
    {
        result |= amendOutputDataIfNeeded(
            accessData, static_cast<nx::vms::api::ResourceParamData*>(&paramData));
    }

    return result;
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    nx::vms::api::FullInfoData* paramData)
{
    return amendOutputDataIfNeeded(accessData, &paramData->allProperties);
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    nx::vms::api::CameraDataEx* paramData)
{
    return amendOutputDataIfNeeded(accessData, &paramData->addParams);
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    nx::vms::api::CameraDataExList* paramDataList)
{
    bool result = false;
    for (auto& paramData: *paramDataList)
        result |= amendOutputDataIfNeeded(accessData, &paramData);

    return result;
}

} // namespace ec2