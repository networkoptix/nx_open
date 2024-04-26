// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/media_server_statistics_manager.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/camera_data_manager.h>
#include <core/resource_management/incompatible_server_watcher.h>
#include <nx/vms/client/desktop/analytics/analytics_entities_tree.h>
#include <nx/vms/client/desktop/analytics/analytics_taxonomy_manager.h>
#include <nx/vms/client/desktop/intercom/intercom_manager.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource/rest_api_helper.h>
#include <nx/vms/client/desktop/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/desktop/settings/system_specific_local_settings.h>
#include <nx/vms/client/desktop/showreel/showreel_state_manager.h>
#include <nx/vms/client/desktop/statistics/statistics_sender.h>
#include <nx/vms/client/desktop/system_administration/watchers/logs_management_watcher.h>
#include <nx/vms/client/desktop/system_administration/watchers/non_editable_users_and_groups.h>
#include <nx/vms/client/desktop/system_administration/watchers/traffic_relay_url_watcher.h>
#include <nx/vms/client/desktop/system_health/default_password_cameras_watcher.h>
#include <nx/vms/client/desktop/system_health/system_health_state.h>
#include <nx/vms/client/desktop/system_logon/logic/delayed_data_loader.h>
#include <nx/vms/client/desktop/utils/ldap_status_watcher.h>
#include <nx/vms/client/desktop/utils/local_file_cache.h>
#include <nx/vms/client/desktop/utils/server_image_cache.h>
#include <nx/vms/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/client/desktop/utils/video_cache.h>
#include <nx/vms/client/desktop/utils/virtual_camera_manager.h>
#include <nx/vms/client/desktop/videowall/videowall_online_screens_watcher.h>
#include <server/server_storage_manager.h>

#include "../system_context.h"

namespace nx::vms::client::desktop {

struct SystemContext::Private
{
    std::unique_ptr<VideoWallOnlineScreensWatcher> videoWallOnlineScreensWatcher;
    std::unique_ptr<LdapStatusWatcher> ldapStatusWatcher;
    std::unique_ptr<QnIncompatibleServerWatcher> incompatibleServerWatcher;
    std::unique_ptr<ServerRuntimeEventConnector> serverRuntimeEventConnector;
    std::unique_ptr<QnServerStorageManager> serverStorageManager;
    std::unique_ptr<QnCameraBookmarksManager> cameraBookmarksManager;
    std::unique_ptr<QnCameraDataManager> cameraDataManager;
    std::unique_ptr<StatisticsSender> statisticsSender;
    std::unique_ptr<VirtualCameraManager> virtualCameraManager;
    std::unique_ptr<VideoCache> videoCache;
    std::unique_ptr<LayoutSnapshotManager> layoutSnapshotManager;
    std::unique_ptr<ShowreelStateManager> showreelStateManager;
    std::unique_ptr<LogsManagementWatcher> logsManagementWatcher;
    std::unique_ptr<QnMediaServerStatisticsManager> mediaServerStatisticsManager;
    std::unique_ptr<SystemSpecificLocalSettings> localSettings;
    std::unique_ptr<RestApiHelper> restApiHelper;
    std::unique_ptr<DelayedDataLoader> delayedDataLoader;
    std::unique_ptr<analytics::TaxonomyManager> taxonomyManager;
    std::unique_ptr<NonEditableUsersAndGroups> nonEditableUsersAndGroups;
    std::unique_ptr<TrafficRelayUrlWatcher> trafficRelayUrlWatcher;
};

} // namespace nx::vms::client::desktop
