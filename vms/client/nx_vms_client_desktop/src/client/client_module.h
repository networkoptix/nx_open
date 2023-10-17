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

namespace nx::vms::license {

class VideoWallLicenseUsageHelper;

} // namespace nx::vms::license

namespace nx::vms::client::desktop {

class AnalyticsAttributeHelper;
class AnalyticsMetadataProviderFactory;
class AnalyticsSettingsManager;
class ClientStateHandler;
class LicenseHealthWatcher;
class RunningInstancesManager;
class SharedMemoryManager;

namespace analytics {
class TaxonomyManager;
class AttributeHelper;
} // namespace analytics

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

    AnalyticsSettingsManager* analyticsSettingsManager() const;

    nx::vms::license::VideoWallLicenseUsageHelper* videoWallLicenseUsageHelper() const;
    analytics::TaxonomyManager* taxonomyManager() const;
    analytics::AttributeHelper* analyticsAttributeHelper() const;

    static void initWebEngine();

private:
    static void initSurfaceFormat();
    void registerResourceDataProviders();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;

    QScopedPointer<AnalyticsMetadataProviderFactory>
        m_analyticsMetadataProviderFactory;

    QScopedPointer<LicenseHealthWatcher> m_licenseHealthWatcher;
};

} // namespace nx::vms::client::desktop

#define qnClientModule nx::vms::client::desktop::QnClientModule::instance()
