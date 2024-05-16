// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/thread/mutex.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/analytics/analytics_entities_tree.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/network/server_primary_interface_watcher.h>
#include <nx/vms/client/core/ptz/client_ptz_controller_pool.h>
#include <nx/vms/client/core/watchers/server_time_watcher.h>
#include <nx/vms/client/core/watchers/user_watcher.h>
#include <nx/vms/client/core/watchers/watermark_watcher.h>
#include <nx/vms/rules/engine_holder.h>

#include "../system_context.h"

namespace nx::vms::client::core {

struct SystemContext::Private
{
    std::unique_ptr<QnPtzControllerPool> ptzControllerPool;
    std::unique_ptr<UserWatcher> userWatcher;
    std::unique_ptr<WatermarkWatcher> watermarkWatcher;
    std::unique_ptr<ServerTimeWatcher> serverTimeWatcher;
    std::unique_ptr<QnServerStorageManager> serverStorageManager;
    std::unique_ptr<ServerRuntimeEventConnector> serverRuntimeEventConnector;
    std::unique_ptr<QnCameraBookmarksManager> cameraBookmarksManager;
    std::unique_ptr<ServerPrimaryInterfaceWatcher> serverPrimaryInterfaceWatcher;
    std::unique_ptr<nx::vms::rules::EngineHolder> vmsRulesEngineHolder;
    std::unique_ptr<core::analytics::AttributeHelper> analyticsAttributeHelper;
    std::unique_ptr<AccessController> accessController;
    std::unique_ptr<analytics::TaxonomyManager> taxonomyManager;
    std::unique_ptr<VideoCache> videoCache;
    std::unique_ptr<AnalyticsEventsSearchTreeBuilder> analyticsEventsSearchTreeBuilder;

    mutable nx::Mutex sessionMutex;

    /** Session this context belongs to. Exclusive with connection. */
    std::shared_ptr<RemoteSession> session;

    /** Connection this context belongs to. Exclusive with session. */
    RemoteConnectionPtr connection;
};

} // namespace nx::vms::client::core
