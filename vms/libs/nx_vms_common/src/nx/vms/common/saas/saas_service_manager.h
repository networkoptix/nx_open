// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <licensing/license_fwd.h>
#include <nx/utils/qt_direct_connect.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/value_cache.h>
#include <nx/utils/scope_guard.h>
#include <nx/vms/api/data/saas_data.h>
#include <nx/vms/common/system_context_aware.h>

namespace nx::vms::common::saas {

class NX_VMS_COMMON_API ServiceManager:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT
    Q_PROPERTY(bool saasActive READ saasActive NOTIFY dataChanged)
    Q_PROPERTY(bool saasShutDown READ saasShutDown NOTIFY dataChanged)
    Q_PROPERTY(bool saasSuspended READ saasSuspended NOTIFY dataChanged)

public:
    ServiceManager(SystemContext* context, QObject* parent = nullptr);

    /**
     * Sets and stores detailed structure describing whole aspects of system's SaaS licensing state.
     * @param saasDataJson nx::vms::api::SaasData structure in JSON representation, expected to be
     *     as response from the licensing server.
     */
    void loadSaasData(const std::string_view& saasDataJson);

    /**
     * Sets and stores services available to the system from Channel Partners.
     * @param servicesJson Array of nx::vms::api::SaasService structures in JSON representation,
     *     expected to be as response from the licensing server
     */
    void loadServiceData(const std::string_view& servicesJson);

    /**
     * @return Detailed structure describing whole aspects of SaaS licensing state of the system.
     */
    nx::vms::api::SaasData data() const;

    /**
     * @return SaaS services available to the system from Channel Partners, indexed by
     *     the service ID.
     */
    std::map<nx::Uuid, nx::vms::api::SaasService> services() const;

    /**
     * @return State of SaaS licensing for the system.
     */
    nx::vms::api::SaasState saasState() const;

    /**
     * Explicitly sets and stores given SaaS services state, setting SaaS state via this method
     * violates data consistency with licensing server. Thus, using this method is expected only
     * for state when licensing server is inaccessible.
     */
    void setSaasStateSync(nx::vms::api::SaasState saasState);

    void setSaasStateAsync(nx::vms::api::SaasState saasState);

    /**
     * @return Parameters of purchased analytics integration services, indexed by service ID.
     */
    std::map<nx::Uuid, nx::vms::api::SaasAnalyticsParameters> analyticsIntegrations() const;

    /**
     * @return Parameters of purchased local recording services, indexed by service ID.
     */
    std::map<nx::Uuid, nx::vms::api::SaasLocalRecordingParameters> localRecording() const;

    /**
     * @return Parameters of purchased live view services, indexed by service ID.
     */
    std::map<nx::Uuid, nx::vms::api::SaasLiveViewParameters> liveView() const;

    /**
     * @return Parameters of purchased cloud storage services, indexed by service ID.
     */
    std::map<nx::Uuid, nx::vms::api::SaasCloudStorageParameters> cloudStorageData() const;

    /**
     * @return Whether Saas is in active state.
     */
    bool saasActive() const;

    /**
     * @return Whether Saas is in suspended state.
     */
    bool saasSuspended() const;

    /**
     * @return Whether SaaS is in suspended or shutdown state. It doesn't matter if shutdown state
     * was received from license server or set due lost connection to a license server.
     */
    bool saasSuspendedOrShutDown() const;

    static bool saasSuspendedOrShutDown(nx::vms::api::SaasState state);

    /**
     * @return Whether SaaS is in shutdown state. It doesn't matter if shutdown state
     * was received from license server or set due lost connection to a license server.
     */
    bool saasShutDown() const;

    static bool saasShutDown(nx::vms::api::SaasState state);

    /**
     * Depending on the SaaS licensing state, adds or removes License object converted from
     * purchased 'local_recording' SaaS services for backward compatibility.
     */
    void updateLocalRecordingLicenseV1();

    /**
     * @return The status of the used SaaS services for the given service type (ok or has an issue).
     */
    nx::vms::api::ServiceTypeStatus serviceStatus(const QString& serviceType) const;

    // Flag which corresponds to 'saasEnabled' flag from the nx_vms_server.ini. Currently doesn't
    // have any sense on the client side.
    void setEnabled(bool value);
    bool isEnabled() const;

    QByteArray rawData() const;


    /**
     * @return true if live service overused
     */
    bool hasLiveServiceOveruse() const;

    /**
     * @return true if live service overused. The value is cached and updated every 5 seconds.
     */
    bool hasLiveServiceOveruseCached() const;

    /**
     * @return true if any of tiers has overuse
     */
    bool hasTierOveruse() const;

    /**
     * @return Tier limitation by provided feature.
     */
    std::optional<int> tierLimit(nx::vms::api::SaasTierLimitName value) const;

    /**
     * @return How many resource by specific tier limit are currently in use.
     */
    int tierLimitUsed(nx::vms::api::SaasTierLimitName value) const;

    /**
     * @return std::nullopt if there is not limit. Otherwise returns delta between tierLimit and
     * used resource.
     */
    std::optional<int> tierLimitLeft(nx::vms::api::SaasTierLimitName value) const;

    std::optional<int> camerasTierLimitLeft(const nx::Uuid& serverId);

    /*
     * @return Whether tier is overused for the requested feature.
     */
    bool tierLimitReached(nx::vms::api::SaasTierLimitName value) const;

    /*
     * @return Whether tier allows the requested feature.
     */
    bool hasFeature(nx::vms::api::SaasTierLimitName value) const;

    /*
     * @return UTC time in milliseconds when grace period is expired.
     * Return 0 if grace period is not started.
     */
    std::chrono::milliseconds tierGracePeriodExpirationTime() const;

    /*
     * @return Whether grace period was started and expired already.
     */
    bool isTierGracePeriodExpired() const;

    /*
     * @return true if grace period was started.
     */
    bool isTierGracePeriodStarted() const;

    /*
     * @return std::null_opt if grace period is not started otherwise returns days left to the end of the grace period.
     * return 0 if grace period is already expired.
    */
    std::optional<int> tierGracePeriodDaysLeft() const;

    /*
     * @return List of overused Tier features. For boolean features fields 'allowed' and 'used'
     * contains only values 1 or 0.
     */
    nx::vms::api::TierOveruseMap tierOveruseDetails() const;

    /*
     * @return statistic for all Tier features. For boolean features fields 'allowed' and 'used'
     * contains only values 1 or 0.
     */
    nx::vms::api::TierUsageMap tiersUsageDetails() const;

    using LocalTierLimits = std::map<nx::vms::api::SaasTierLimitName, std::optional<int>>;

    /*
     * Used for test purpose only.
     */
    void setLocalTierLimits(const LocalTierLimits& value);

    /*
     * Used for test purpose only.
     */
    LocalTierLimits localTierLimits() const;

signals:
    void saasStateChanged();
    void dataChanged();
    void saasShutDownChanged();
    void saasSuspendedChanged();

private:
    QnLicensePtr localRecordingLicenseV1Unsafe() const;
    void updateLocalRecordingLicenseV1Unsafe();

    void setData(
        nx::vms::api::SaasData data,
        LocalTierLimits localTierLimits);
    void setServices(const std::vector<nx::vms::api::SaasService>& services);

    /*
     * @return purshase info by serviceId
     */
    template <typename ServiceParamsType>
    std::map<nx::Uuid, ServiceParamsType> purchasedServices(const QString& serviceType) const;
    void setSaasStateInternal(api::SaasState saasState, bool waitForDone);

    std::map<nx::Uuid, int> overusedResourcesUnsafe(
        nx::vms::api::SaasTierLimitName feature) const;
    std::chrono::milliseconds calculateGracePeriodTime() const;
    void resetGracePeriodCache();
    std::optional<int> tierLimitUnsafe(nx::vms::api::SaasTierLimitName value) const;

private:
    mutable nx::Mutex m_mutex;
    nx::vms::api::SaasData m_data;
    std::map<nx::Uuid, nx::vms::api::SaasService> m_services;
    std::atomic<bool> m_enabled{false};
    std::vector<nx::utils::SharedGuardPtr> m_qtSignalGuards;

    nx::utils::CachedValue<std::chrono::milliseconds> m_tierGracePeriodExpirationTime;
    nx::utils::CachedValueWithTimeout<bool> m_liveOveruse;
};

} // nx::vms::common::saas
