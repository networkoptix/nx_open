// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <licensing/license_fwd.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/api/data/saas_data.h>
#include <nx/vms/common/system_context_aware.h>

namespace nx::vms::common::saas {

class NX_VMS_COMMON_API ServiceManager:
    public QObject,
    public SystemContextAware
{
    Q_OBJECT

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
    std::map<QnUuid, nx::vms::api::SaasService> services() const;

    /**
     * @return State of SaaS licensing for the system.
     */
    nx::vms::api::SaasState saasState() const;

    /**
     * Explicitly sets and stores given SaaS services state, setting SaaS state via this method
     * violates data consistency with licensing server. Thus, using this method is expected only
     * for state when licensing server is inaccessible.
     */
    void setSaasState(nx::vms::api::SaasState saasState);

    /**
     * @return Parameters of purchased analytics integration services, indexed by service ID.
     */
    std::map<QnUuid, nx::vms::api::SaasAnalyticsParameters> analyticsIntegrations() const;

    /**
     * @return Parameters of purchased local recording services, indexed by service ID.
     */
    std::map<QnUuid, nx::vms::api::SaasLocalRecordingParameters> localRecording() const;

    /**
     * @return Parameters of purchased cloud storage services, indexed by service ID.
     */
    std::map<QnUuid, nx::vms::api::SaasCloudStorageParameters> cloudStorageData() const;

    /**
     * @return Whether access to the system is blocked to cloud users with the reason description.
     */
    std::pair<bool, QString> isBlocked() const;

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
    
signals:
    void dataChanged();

private:
    QnLicensePtr localRecordingLicenseV1Unsafe() const;
    void updateLocalRecordingLicenseV1Unsafe();

    void setData(const nx::vms::api::SaasData& data);
    void setServices(const std::vector<nx::vms::api::SaasService>& services);

    template <typename ServiceParamsType>
    std::map<QnUuid, ServiceParamsType> purchasedServices(const QString& serviceType) const;

private:
    mutable nx::Mutex m_mutex;
    nx::vms::api::SaasData m_data;
    std::map<QnUuid, nx::vms::api::SaasService> m_services;
    std::atomic<bool> m_enabled{false};
};

} // nx::vms::common::saas
