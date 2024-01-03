// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "site_information.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

SiteInformation::SiteInformation(const ModuleInformation& module):
    name(module.systemName),
    customization(module.customization),
    version(module.version),
    protoVersion(module.protoVersion),
    cloudHost(module.cloudHost),
    cloudOwnerId(module.cloudOwnerId),
    organizationId(module.organizationId),
    synchronizedTimeMs(module.synchronizedTimeMs)
{
    if (!module.localSystemId.isNull())
        localId = module.localSystemId;

    if (!module.cloudSystemId.isEmpty())
        cloudId = module.cloudSystemId;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SiteInformation, (json), SiteInformation_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(OtherSiteRequest, (json), OtherSiteRequest_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(LocalSiteAuth, (json), LocalSystemAuth_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CloudSiteAuth, (json), CloudSiteAuth_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CloudSystemAuth, (json), CloudSystemAuth_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SetupSiteData, (json), SetupSiteData_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SetupSystemData, (json), SetupSystemData_Fields)

} // namespace nx::vms::api
