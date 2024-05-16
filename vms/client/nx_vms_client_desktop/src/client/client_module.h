// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <client/client_startup_parameters.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/radass/radass_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>

class QnClientCoreModule;

namespace nx::vms::api { enum class PeerType; }
namespace nx::vms::license { class VideoWallLicenseUsageHelper; }

namespace nx::vms::client::core::analytics {
class AnalyticsMetadataProviderFactory;
class AttributeHelper;
class TaxonomyManager;
} // namespace nx::vms::client::core::analytics

namespace nx::vms::client::core {
class AnalyticsSettingsManager;
class AnalyticsAttributeHelper;
} // namespace nx::vms::client::core

namespace nx::vms::client::desktop {

class ClientStateHandler;
class LicenseHealthWatcher;
class RunningInstancesManager;
class SharedMemoryManager;

class NX_VMS_CLIENT_DESKTOP_API QnClientModule: public QObject, public SystemContextAware
{
    Q_OBJECT

public:
    explicit QnClientModule(
        SystemContext* systemContext,
        const QnStartupParameters& startupParams,
        QObject* parent = nullptr);
    virtual ~QnClientModule() override;

    static QnClientModule* instance();

    void startLocalSearchers();

    QnClientCoreModule* clientCoreModule() const;

    QnStartupParameters startupParameters() const;

    core::AnalyticsSettingsManager* analyticsSettingsManager() const;

    license::VideoWallLicenseUsageHelper* videoWallLicenseUsageHelper() const;

    static void initWebEngine();

private:
    static void initSurfaceFormat();
    void registerResourceDataProviders();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop

#define qnClientModule nx::vms::client::desktop::QnClientModule::instance()
