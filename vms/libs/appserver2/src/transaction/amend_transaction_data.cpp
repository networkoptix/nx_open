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
#include <core/resource_access/resource_access_manager.h>
#include <nx/vms/event/action_parameters.h>
#include <nx/fusion/serialization/json.h>

namespace ec2 {

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* /*accessManager*/,
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
            paramData->value = decryptedValue.left(decryptedValue.indexOf(':')) + ":" + kHiddenPasswordFiller;

        return true;
    }

    return false;
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::EventRuleData* rule)
{
    nx::vms::event::ActionParameters params;
    if (!QJson::deserialize<nx::vms::event::ActionParameters>(rule->actionParams, &params))
        return false;

    nx::utils::Url url = params.url;
    if (url.password().isEmpty())
        return false;

    bool isGranted = accessData == Qn::kSystemAccess
        || accessManager->hasGlobalPermission(accessData, GlobalPermission::admin);
    if (isGranted)
        url.setPassword(nx::utils::decodeStringFromHexStringAES128CBC(url.password()));
    else
        url.setPassword(kHiddenPasswordFiller);
    params.url = url.toString();
    rule->actionParams = QJson::serialized(params);
    return true;
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::EventRuleDataList* ruleList)
{
    bool result = false;
    for (auto& rule: *ruleList)
        result |= amendOutputDataIfNeeded(accessData, accessManager, &rule);

    return result;
}


bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::ResourceParamWithRefData* paramData)
{
    return amendOutputDataIfNeeded(
        accessData, accessManager, static_cast<nx::vms::api::ResourceParamData*>(paramData));
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    std::vector<nx::vms::api::ResourceParamData>* paramDataList)
{
    bool result = false;
    for (auto& paramData : *paramDataList)
        result |= amendOutputDataIfNeeded(accessData, accessManager, &paramData);

    return result;
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    std::vector<nx::vms::api::ResourceParamWithRefData>* paramWithRefDataList)
{
    bool result = false;
    for (auto& paramData : *paramWithRefDataList)
    {
        result |= amendOutputDataIfNeeded(
            accessData, accessManager, static_cast<nx::vms::api::ResourceParamData*>(&paramData));
    }

    return result;
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::FullInfoData* paramData)
{
    auto result = amendOutputDataIfNeeded(accessData, accessManager, &paramData->allProperties);
    result |= amendOutputDataIfNeeded(accessData, accessManager, &paramData->rules);
    return result;
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::CameraDataEx* paramData)
{
    return amendOutputDataIfNeeded(accessData, accessManager, &paramData->addParams);
}

bool amendOutputDataIfNeeded(const Qn::UserAccessData& accessData,
    QnResourceAccessManager* accessManager,
    nx::vms::api::CameraDataExList* paramDataList)
{
    bool result = false;
    for (auto& paramData: *paramDataList)
        result |= amendOutputDataIfNeeded(accessData, accessManager, &paramData);

    return result;
}

} // namespace ec2