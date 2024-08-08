// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/http_client_pool.h>
#include <api/runtime_info_manager.h>
#include <core/resource/camera_history.h>
#include <core/resource_access/access_rights_manager.h>
#include <core/resource_access/global_permissions_watcher.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject_hierarchy.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_properties.h>
#include <core/resource_management/server_additional_addresses_dictionary.h>
#include <core/resource_management/status_dictionary.h>
#include <licensing/license.h>
#include <nx/analytics/taxonomy/descriptor_container.h>
#include <nx/analytics/taxonomy/state_watcher.h>
#include <nx/metrics/metrics_storage.h>
#include <nx/vms/common/license/license_usage_watcher.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/common/network/abstract_certificate_verifier.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/common/user_management/user_group_manager.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/event/rule_manager.h>
#include <utils/camera/camera_names_watcher.h>

#include "../system_context.h"

namespace nx::vms::common {

struct SystemContext::Private
{
    const Mode mode;
    const nx::Uuid peerId;
    QnCommonMessageProcessor* messageProcessor = nullptr;

    std::unique_ptr<QnLicensePool> licensePool;
    std::unique_ptr<saas::ServiceManager> saasServiceManager;
    std::unique_ptr<QnResourcePool> resourcePool;
    std::unique_ptr<QnResourceDataPool> resourceDataPool;
    std::unique_ptr<QnResourceStatusDictionary> resourceStatusDictionary;
    std::unique_ptr<QnResourcePropertyDictionary> resourcePropertyDictionary;
    std::unique_ptr<QnCameraHistoryPool> cameraHistoryPool;
    std::unique_ptr<network::http::ClientPool> httpClientPool;
    std::unique_ptr<QnServerAdditionalAddressesDictionary> serverAdditionalAddressesDictionary;
    std::unique_ptr<QnRuntimeInfoManager> runtimeInfoManager;
    std::unique_ptr<SystemSettings> globalSettings;
    std::unique_ptr<UserGroupManager> userGroupManager;
    std::unique_ptr<nx::core::access::ResourceAccessSubjectHierarchy> accessSubjectHierarchy;
    std::unique_ptr<nx::core::access::GlobalPermissionsWatcher> globalPermissionsWatcher;
    std::unique_ptr<nx::core::access::AccessRightsManager> accessRightsManager;
    std::unique_ptr<QnResourceAccessManager> resourceAccessManager;
    std::unique_ptr<ShowreelManager> showreelManager;
    std::unique_ptr<LookupListManager> lookupListManager;
    std::unique_ptr<nx::vms::event::RuleManager> eventRuleManager;
    std::unique_ptr<nx::analytics::taxonomy::DescriptorContainer> analyticsDescriptorContainer;
    std::unique_ptr<nx::analytics::taxonomy::AbstractStateWatcher> analyticsTaxonomyStateWatcher;
    std::shared_ptr<nx::metrics::Storage> metrics;
    std::unique_ptr<DeviceLicenseUsageWatcher> deviceLicenseUsageWatcher;
    std::unique_ptr<VideoWallLicenseUsageWatcher> videoWallLicenseUsageWatcher;

    QPointer<AbstractCertificateVerifier> certificateVerifier;
    QPointer<nx::vms::discovery::Manager> moduleDiscoveryManager;
    nx::vms::rules::Engine* vmsRulesEngine = {};
    std::unique_ptr<QnCameraNamesWatcher> cameraNamesWatcher;
};

} // namespace nx::vms::common
