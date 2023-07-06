// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <licensing/license_fwd.h>
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

    nx::vms::api::SaasData data() const;
    std::map<QnUuid, nx::vms::api::SaasService> services() const;

    bool loadSaasData(const std::string_view& saasData);
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

    // Set `disabled` flag. It is needed for debugging via ini config.
    void setDisabled();
    bool isEnabled() const;

signals:
    void dataChanged();

private:
    void onDataChanged();

    template <typename ServiceParamsType>
    std::map<QnUuid, ServiceParamsType>  purchasedServices(const QString& serviceType) const;

private:
    nx::vms::api::SaasData m_data;
    std::map<QnUuid, nx::vms::api::SaasService> m_services;
    std::atomic<bool> m_disabled{false};

};

} // nx::vms::common::saas
