// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "saas_data.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

const int SaasCloudStorageParameters::kUnlimitedResolution = std::numeric_limits<int>::max();
static const QString kSaasDateTimeFormat("yyyy-MM-dd hh:mm:ss");

bool fromString(const std::string& value, SaasDateTime* target)
{
    *target = QDateTime::fromString(QString::fromUtf8(value), kSaasDateTimeFormat);
    target->setTimeZone(QTimeZone::UTC);
    return true;
}

QString SaasDateTime::toString() const
{
    return QDateTime::toString(kSaasDateTimeFormat);
}

const QString SaasService::kLocalRecordingServiceType("local_recording");
const QString SaasService::kAnalyticsIntegrationServiceType("analytics");
const QString SaasService::kCloudRecordingType("cloud_storage");
const QString SaasService::kLiveViewServiceType("live_view");

template <typename T>
struct Field
{
    T* value;
    const char* name;
};

template <typename T>
void assignField(const SaasServiceParameters& parameters, const Field<T>& field)
{
    try
    {
        if (auto it = parameters.find(field.name); it != parameters.end())
        {
            if constexpr (std::is_same<T, nx::Uuid>::value)
                *field.value = nx::Uuid(std::get<std::string>(it->second));
            else
                *field.value = std::get<T>(it->second);
        }
    }
    catch (const std::bad_variant_access& ex)
    {
        NX_WARNING(NX_SCOPE_TAG,
            "Unexpected type for analitics service parameter. %1", ex.what());
    }
}

#define fieldFromMap(result, parameters, field) \
    assignField(parameters, Field<decltype(result.field)>{&result.field, #field});

SaasLocalRecordingParameters SaasLocalRecordingParameters::fromParams(const SaasServiceParameters& parameters)
{
    SaasLocalRecordingParameters result;
    fieldFromMap(result, parameters, totalChannelNumber);
    return result;
}

SaasLiveViewParameters SaasLiveViewParameters::fromParams(
    const SaasServiceParameters& parameters)
{
    SaasLiveViewParameters result;
    fieldFromMap(result, parameters, totalChannelNumber);
    return result;
}

SaasAnalyticsParameters SaasAnalyticsParameters::fromParams(const SaasServiceParameters& parameters)
{
    SaasAnalyticsParameters result;
    fieldFromMap(result, parameters, totalChannelNumber);
    fieldFromMap(result, parameters, integrationId);
    return result;
}

SaasCloudStorageParameters SaasCloudStorageParameters::fromParams(const SaasServiceParameters& parameters)
{
    SaasCloudStorageParameters result;
    fieldFromMap(result, parameters, totalChannelNumber);
    fieldFromMap(result, parameters, days);
    fieldFromMap(result, parameters, maxResolutionMp);
    if (result.maxResolutionMp == 0)
        result.maxResolutionMp = kUnlimitedResolution;
    return result;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SaasLocalRecordingParameters, (json), SaasLocalRecordingParameters_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SaasCloudStorageParameters, (json), SaasCloudStorageParameters_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SaasAnalyticsParameters, (json), SaasAnalyticsParameters_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SaasLiveViewParameters, (json), SaasLiveViewParameters_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SaasService, (json), SaasService_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SaasPurchase, (json), SaasPurchase_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ServiceTypeStatus, (json), ServiceTypeStatus_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ChannelPartnerSupportContactInfoRecord, (json), ChannelPartnerSupportContactInfoRecord_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ChannelPartnerSupportCustomInfo, (json), ChannelPartnerSupportCustomInfo_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ChannelPartnerSupportInformation, (json), ChannelPartnerSupportInformation_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ChannelPartner, (json), ChannelPartner_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Organization, (json), Organization_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SaasSecurity, (json), SaasSecurity_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SaasData, (json), SaasData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SaasWithServices, (json), SaasWithServices_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TierUsageData, (json), TierUsageData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(TierOveruseData, (json), TierOveruseData_Fields)

} // namespace nx::vms::api
