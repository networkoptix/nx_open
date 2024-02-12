// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>

struct QnCloudSystem;

namespace nx::vms::api { struct ModuleInformation; }
namespace nx::vms::common { class SystemContext; }

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

NX_VMS_COMMON_API nx::Uuid getLocalSystemId(const nx::vms::api::ModuleInformation& info);

NX_VMS_COMMON_API bool isNewSystem(const nx::vms::api::ModuleInformation& info);
NX_VMS_COMMON_API bool isNewSystem(const QnCloudSystem& info);

NX_VMS_COMMON_API bool isCloudSystem(const nx::vms::api::ModuleInformation& info);

NX_VMS_COMMON_API QString getSystemName(const nx::vms::api::ModuleInformation& info);

NX_VMS_COMMON_API nx::Uuid currentSystemLocalId(const nx::vms::common::SystemContext* context);

NX_VMS_COMMON_API bool currentSystemIsNew(const nx::vms::common::SystemContext* context);

NX_VMS_COMMON_API bool serverBelongsToCurrentSystem(
    const nx::vms::api::ModuleInformation& info,
    const nx::vms::common::SystemContext* context);

NX_VMS_COMMON_API bool serverBelongsToCurrentSystem(const QnMediaServerResourcePtr& server);

/** Server address to access it using cloud sockets. */
NX_VMS_COMMON_API QString serverCloudHost(const QString& systemId, const nx::Uuid& serverId);

} // helpers
