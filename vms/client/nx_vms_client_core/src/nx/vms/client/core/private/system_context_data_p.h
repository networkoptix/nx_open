// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <camera/camera_bookmarks_manager.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/analytics/analytics_attribute_helper.h>
#include <nx/vms/client/core/analytics/analytics_entities_tree.h>
#include <nx/vms/client/core/camera/camera_data_manager.h>
#include <nx/vms/client/core/io_ports/io_ports_compatibility_interface.h>
#include <nx/vms/client/core/network/network_module.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/network/remote_session_timeout_watcher.h>
#include <nx/vms/client/core/network/server_primary_interface_watcher.h>
#include <nx/vms/client/core/ptz/client_ptz_controller_pool.h>
#include <nx/vms/client/core/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/core/utils/deployment_manager.h>
#include <nx/vms/client/core/utils/video_cache.h>
#include <nx/vms/client/core/watchers/feature_access_watcher.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/traffic_relay_url_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/core/watchers/watermark_watcher.h>
#include <nx/vms/rules/engine_holder.h>
#include <storage/server_storage_manager.h>

#include "../system_context.h"

namespace nx::vms::client::core {

class RemoteAsyncImageProvider;

struct SystemContext::Private
{
    void initializeIoPortsInterface();

    void handleConnectionChanged(const RemoteConnectionPtr& oldConnection,
        const RemoteConnectionPtr& newConnection);

    SystemContext* const q;

    mutable std::unique_ptr<QnPtzControllerPool> ptzControllerPool;
    std::unique_ptr<UserWatcher> userWatcher;
    std::unique_ptr<WatermarkWatcher> watermarkWatcher;
    std::unique_ptr<ServerTimeWatcher> serverTimeWatcher;
    std::unique_ptr<QnServerStorageManager> serverStorageManager;
    std::unique_ptr<ServerRuntimeEventConnector> serverRuntimeEventConnector;
    std::unique_ptr<QnCameraBookmarksManager> cameraBookmarksManager;
    std::unique_ptr<CameraDataManager> cameraDataManager;
    std::unique_ptr<ServerPrimaryInterfaceWatcher> serverPrimaryInterfaceWatcher;
    std::unique_ptr<nx::vms::rules::EngineHolder> vmsRulesEngineHolder;
    std::unique_ptr<core::analytics::AttributeHelper> analyticsAttributeHelper;
    std::unique_ptr<AccessController> accessController;
    std::unique_ptr<analytics::TaxonomyManager> taxonomyManager;
    std::unique_ptr<VideoCache> videoCache;
    std::unique_ptr<AnalyticsEventsSearchTreeBuilder> analyticsEventsSearchTreeBuilder;
    std::unique_ptr<RemoteSessionTimeoutWatcher> sessionTimeoutWatcher;
    std::unique_ptr<TrafficRelayUrlWatcher> trafficRelayUrlWatcher;
    std::unique_ptr<FeatureAccessWatcher> featureAccessWatcher;
    std::unique_ptr<DeploymentManager> deploymentManager;

    mutable nx::Mutex sessionMutex;

    /** Session this context belongs to. Exclusive with connection. */
    std::shared_ptr<RemoteSession> session;

    /** Connection this context belongs to. Exclusive with session. */
    RemoteConnectionPtr connection;

    // Should be destroyed before session/connection.
    std::unique_ptr<IoPortsCompatibilityInterface> ioPortsInterface;
};

} // namespace nx::vms::client::core
