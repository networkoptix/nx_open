// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "feature_access_watcher.h"

#include <nx/vms/client/core/application_context.h>
#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/settings/client_core_settings.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/watchers/cloud_features_watcher.h>

namespace nx::vms::client::core {

namespace {

static const utils::SoftwareVersion kSite61Version(6, 1);

struct CloudSiteFilter
{
    utils::SoftwareVersion minSiteVersion;
    bool belongsToOrganization = false;
};

} // namespace

struct FeatureAccessWatcher::Private
{
    FeatureAccessWatcher* const q;

    bool canUseShareBookmark = false;
    bool canUseDeployByQrFeature = false;
    bool canUseCrashReporting = false;

    bool isAcceptableCloudSite(const CloudSiteFilter& filter) const;

    void updateCanUseShareBookmark();
    void updateCanUseDeployByQrFeature();
    void updateCanUseCrashReporting();
};

bool FeatureAccessWatcher::Private::isAcceptableCloudSite(const CloudSiteFilter& filter) const
{
    const auto context = q->systemContext();
    if (!context)
        return false;

    const auto session = context->session();
    const auto connection = session
        ? session->connection()
        : context->connection();
    if (!connection || !connection->connectionInfo().isCloud())
        return false;

    const auto moduleInformation = context->moduleInformation();
    return moduleInformation.version >= filter.minSiteVersion
        && (!filter.belongsToOrganization || moduleInformation.organizationId);
}

void FeatureAccessWatcher::Private::updateCanUseShareBookmark()
{
    const bool newValue = isAcceptableCloudSite(
        {.minSiteVersion = kSite61Version, .belongsToOrganization = true});

    if (newValue == canUseShareBookmark)
        return;

    canUseShareBookmark = newValue;
    emit q->canUseShareBookmarkChanged();
}

void FeatureAccessWatcher::Private::updateCanUseDeployByQrFeature()
{
    const bool allowedByFeatureFlag = appContext()->cloudFeaturesWatcher()->hasFeature(
        CloudFeature::vmsClientQrCodeDeployment);
    const bool newValue =
        (allowedByFeatureFlag || appContext()->coreSettings()->forceDeployByQrCodeFeature())
        && isAcceptableCloudSite({.minSiteVersion = kSite61Version, .belongsToOrganization = true});

    if (newValue == canUseDeployByQrFeature)
        return;

    canUseDeployByQrFeature = newValue;
    emit q->canUseDeployByQrFeatureChanged();
}

FeatureAccessWatcher::FeatureAccessWatcher(SystemContext* context, QObject* parent):
    QObject(parent),
    SystemContextAware(context),
    d(new Private{.q = this})
{
    const auto updateFeaturesAvailability =
        [this]()
        {
            d->updateCanUseShareBookmark();
            d->updateCanUseDeployByQrFeature();
        };

    connect(context, &SystemContext::remoteIdChanged,
        this, updateFeaturesAvailability);
    connect(context, &SystemContext::credentialsChanged,
        this, updateFeaturesAvailability);

    const auto settings = appContext()->coreSettings();
    connect(settings, &vms::client::core::Settings::changed, this,
        [this, settings](const auto property)
        {
            if (property && property->name == settings->forceDeployByQrCodeFeature.name)
                d->updateCanUseDeployByQrFeature();
        });

    connect(appContext()->cloudFeaturesWatcher(), &CloudFeaturesWatcher::featuresChanged, this,
        [this](auto changedFeatures)
        {
            if (changedFeatures.testFlag(CloudFeature::vmsClientQrCodeDeployment))
                d->updateCanUseDeployByQrFeature();
        });
    updateFeaturesAvailability();
}

FeatureAccessWatcher::~FeatureAccessWatcher() = default;

bool FeatureAccessWatcher::canUseShareBookmark() const
{
    return d->canUseShareBookmark;
}

bool FeatureAccessWatcher::canUseDeployByQrFeature() const
{
    return d->canUseDeployByQrFeature;
}

} // namespace nx::vms::client::core
