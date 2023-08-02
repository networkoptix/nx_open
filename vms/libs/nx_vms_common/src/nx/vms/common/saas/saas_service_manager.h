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
     *  Full SaaS data.
     */
    nx::vms::api::SaasData data() const;
    
    std::pair<bool, QString> isBlocked() const;

    std::map<QnUuid, nx::vms::api::SaasService> services() const;

    bool loadSaasData(const std::string_view& saasData);
    void loadSaasData(const nx::vms::api::SaasData& saasData);

    bool loadServiceData(const std::string_view& serviceData);

    /**
     *  Convert 'localRecording' saas services to the License object for
     *  backward compatibility.
     */
    QnLicensePtr localRecordingLicenseV1() const;

    /**
     *  @return map of analytics integration and their parameters.
     *  key - serviceId, value - analytics integration parameters.
     */
    std::map<QnUuid, nx::vms::api::SaasAnalyticsParameters> analyticsIntegrations() const;

    /**
     *  @return map of analytics integration and their parameters.
     *  key - serviceId, value - local recording parameters.
     */
    std::map<QnUuid, nx::vms::api::SaasLocalRecordingParameters> localRecording() const;

    /**
     *  @return data about existing cloud storage services. key - serviceId, value - parameters.
     */
    std::map<QnUuid, nx::vms::api::SaasCloudStorageParameters> cloudStorageData() const;

    // Set `enabled` flag. It is needed for debugging via ini config.
    void setEnabled(bool value);
    bool isEnabled() const;
    
    void updateLicenseV1();

    void setState(nx::vms::api::SaasState state);
    nx::vms::api::SaasState state() const;

    /**
     * @return The status of the service (ok or has an issue).
     */
    nx::vms::api::ServiceTypeStatus serviceState(const QString& serviceName) const;

signals:
    void dataChanged();

private:
    void setData(const nx::vms::api::SaasData& data);
    void saveSaasDataToProperty(const std::string_view& data);
    void onDataChanged();
    QnLicensePtr localRecordingLicenseV1Unsafe() const;

    template <typename ServiceParamsType>
    std::map<QnUuid, ServiceParamsType>  purchasedServices(const QString& serviceType) const;

private:
    mutable nx::Mutex m_mutex;
    nx::vms::api::SaasData m_data;
    std::map<QnUuid, nx::vms::api::SaasService> m_services;
    std::atomic<bool> m_enabled{false};

};

} // nx::vms::common::saas
