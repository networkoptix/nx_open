// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>

struct QnCloudSystem;
class QnCommonModule;
class QnUuid;

namespace nx::vms::api {

struct ModuleInformation;

} // namespace nx::vms::api

namespace helpers {

static constexpr int kDefaultConnectionPort = 7001;
static const QString kFactorySystemUser = "admin";
static const QString kFactorySystemPassword = "admin";

/*
* Extracts system id. Result is:
* - identifier of server if it is in "new state"
* - cloud id if server in cloud
* - system name in other cases
*/
NX_VMS_COMMON_API QString getTargetSystemId(const nx::vms::api::ModuleInformation& info);
NX_VMS_COMMON_API QString getTargetSystemId(const QnCloudSystem& cloudSystem);

NX_VMS_COMMON_API QnUuid getLocalSystemId(const nx::vms::api::ModuleInformation& info);

NX_VMS_COMMON_API bool isNewSystem(const nx::vms::api::ModuleInformation& info);
NX_VMS_COMMON_API bool isNewSystem(const QnCloudSystem& info);

NX_VMS_COMMON_API bool isCloudSystem(const nx::vms::api::ModuleInformation& info);

NX_VMS_COMMON_API QString getSystemName(const nx::vms::api::ModuleInformation& info);

NX_VMS_COMMON_API QnUuid currentSystemLocalId(const QnCommonModule* commonModule);

NX_VMS_COMMON_API bool currentSystemIsNew(const QnCommonModule* commonModule);

NX_VMS_COMMON_API bool serverBelongsToCurrentSystem(
    const nx::vms::api::ModuleInformation& info,
    const QnCommonModule* commonModule);

NX_VMS_COMMON_API bool serverBelongsToCurrentSystem(const QnMediaServerResourcePtr& server);

} // helpers
