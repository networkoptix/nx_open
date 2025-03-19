// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/mobile/window_context_aware.h>

Q_MOC_INCLUDE("nx/vms/client/core/event_search/models/event_search_model_adapter.h")
Q_MOC_INCLUDE("nx/vms/client/core/two_way_audio/two_way_audio_controller.h")
Q_MOC_INCLUDE("nx/vms/client/core/event_search/utils/common_object_search_setup.h")
Q_MOC_INCLUDE("nx/vms/client/core/event_search/utils/analytics_search_setup.h")

class QnAvailableCamerasWatcher;
class QnCameraThumbnailCache;
class QnResourceDiscoveryManager;

namespace nx::client::mobile { class EventRulesWatcher; }

namespace nx::vms::client::core
{
class EventSearchModelAdapter;
class TwoWayAudioController;
class CommonObjectSearchSetup;
class AnalyticsSearchSetup;
}

namespace nx::vms::client::mobile {

class MediaDownloadManager;
class SessionManager;
class WindowContext;

class SystemContext: public core::SystemContext,
    public WindowContextAware
{
    Q_OBJECT

    using base_type = core::SystemContext;

    Q_PROPERTY(core::TwoWayAudioController* twoWayAudioController
        READ twoWayAudioController
        CONSTANT)

public:
    /**
     * @see nx::vms::client::core::SystemContext
     */
    SystemContext(WindowContext* context,
        Mode mode,
        nx::Uuid peerId,
        QObject* parent = nullptr);

    virtual ~SystemContext() override;

    QnAvailableCamerasWatcher* availableCamerasWatcher() const;

    QnResourceDiscoveryManager* resourceDiscoveryManager() const;

    core::TwoWayAudioController* twoWayAudioController() const;

    QnCameraThumbnailCache* cameraThumbnailCache() const;

    nx::client::mobile::EventRulesWatcher* eventRulesWatcher() const;

    // Invokables.

    Q_INVOKABLE core::EventSearchModelAdapter* createSearchModel(bool analyticsMode);

    Q_INVOKABLE core::CommonObjectSearchSetup* createSearchSetup(
        core::EventSearchModelAdapter* model);

    Q_INVOKABLE core::AnalyticsSearchSetup* createAnalyticsSearchSetup(
        core::EventSearchModelAdapter* model);

    Q_INVOKABLE bool hasViewBookmarksPermission();

    Q_INVOKABLE bool hasSearchObjectsPermission();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
