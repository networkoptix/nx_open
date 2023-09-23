// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <client/client_startup_parameters.h>
#include <nx/utils/impl_ptr.h>
#include <nx/vms/client/desktop/radass/radass_fwd.h>

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
class SystemContext;

namespace analytics {
class TaxonomyManager;
class AttributeHelper;
} // namespace analytics

} // namespace nx::vms::client::desktop

class NX_VMS_CLIENT_DESKTOP_API QnClientModule: public QObject
{
    Q_OBJECT

public:
    explicit QnClientModule(
        const QnStartupParameters& startupParams,
        QObject* parent = nullptr);
    virtual ~QnClientModule() override;

    static QnClientModule* instance();

    void startLocalSearchers();

    QnClientCoreModule* clientCoreModule() const;

    nx::vms::client::desktop::SystemContext* systemContext() const;

    QnStartupParameters startupParameters() const;

    nx::vms::client::desktop::AnalyticsSettingsManager* analyticsSettingsManager() const;

    nx::vms::license::VideoWallLicenseUsageHelper* videoWallLicenseUsageHelper() const;
    nx::vms::client::desktop::analytics::TaxonomyManager* taxonomyManager() const;
    nx::vms::client::desktop::analytics::AttributeHelper* analyticsAttributeHelper() const;

    static void initWebEngine();

private:
    static void initSurfaceFormat();
    void registerResourceDataProviders();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;

    QScopedPointer<nx::vms::client::desktop::AnalyticsMetadataProviderFactory>
        m_analyticsMetadataProviderFactory;

    QScopedPointer<nx::vms::client::desktop::LicenseHealthWatcher> m_licenseHealthWatcher;
};

#define qnClientModule QnClientModule::instance()
