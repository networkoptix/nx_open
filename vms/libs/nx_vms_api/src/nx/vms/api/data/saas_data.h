// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <variant>

#include <QtCore/QDateTime>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/software_version.h>
#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx::vms::api {

class NX_VMS_API SaasDateTime: public QDateTime
{
public:
    SaasDateTime() = default;
    SaasDateTime(const QDateTime& value): QDateTime(value) {}

    QString toString() const;
};

bool NX_VMS_API fromString(const std::string& value, SaasDateTime* target);
NX_REFLECTION_TAG_TYPE(SaasDateTime, useStringConversionForSerialization)

/**%apidoc Saas service state */
NX_REFLECTION_ENUM_CLASS(SaasServiceState,
    /**%apidoc active */
    active,

    /**%apidoc disabled */
    obsolete
);

using SaasServiceParameters = std::map<std::string, std::variant<QString, int>>;

/**%apidoc Saas service parameters for 'localRecording' service type */
struct NX_VMS_API SaasLocalRecordingParameters
{
    /**%apidoc Amount of channels */
    int totalChannelNumber = 1;

    static SaasLocalRecordingParameters fromParams(const SaasServiceParameters& parameters);
};
#define SaasLocalRecordingParameters_fields (totalChannelNumber)
NX_REFLECTION_INSTRUMENT(SaasLocalRecordingParameters, SaasLocalRecordingParameters_fields)

/**%apidoc Saas service parameters for 'cloudStorage' service type */
struct NX_VMS_API SaasCloudStorageParameters
{
    static const int kUnlimitedResolution;

    /**%apidoc Amount of channels */
    int totalChannelNumber = 1;

    /**%apidoc Archive duration in days */
    int days = 0;

    /**%apidoc Maximum camera resolution in megapixels */
    int maxResolutionMp = kUnlimitedResolution;

    static SaasCloudStorageParameters fromParams(const SaasServiceParameters& parameters);
};
#define SaasCloudStorageParameters_fields (totalChannelNumber)(days)(maxResolutionMp)
NX_REFLECTION_INSTRUMENT(SaasCloudStorageParameters, SaasCloudStorageParameters_fields)

/**%apidoc Saas service parameters for 'analytics' service type */
struct NX_VMS_API SaasAnalyticsParameters
{
    int totalChannelNumber = 1;

    /**%apidoc Analytics Plugin id. */
    QString integrationId;

    static SaasAnalyticsParameters fromParams(const SaasServiceParameters& parameters);
};
#define SaasAnalyticsParameters_fields (totalChannelNumber)(integrationId)
NX_REFLECTION_INSTRUMENT(SaasAnalyticsParameters, SaasAnalyticsParameters_fields)

/**%apidoc SaasService */
struct NX_VMS_API SaasService
{
    static const QString kLocalRecordingServiceType;
    static const QString kAnalyticsIntegrationServiceType;
    static const QString kCloudRecordingType;

    /**%apidoc Internal service identifier */
    QnUuid id;

    /**%apidoc service type. Service parameters depends on service type. */
    QString type;

    /**%apidoc Service state (active or obsolete). */
    SaasServiceState state = SaasServiceState::active;

    /**%apidoc Human readable name of service. */
    QString displayName;

    /**%apidoc Description of service for Channel partner's users. */
    QString description;

    /**%apidoc Identifier of Brand owner or Channel partner issuing the service. */
    QnUuid createdByChannelPartner;

    /**%apidoc Configurable service parameters.  Different block for each service type. */
    SaasServiceParameters parameters;

    bool operator==(const SaasService&) const = default;
};
#define SaasService_fields (id)(type)(state)(displayName)(description)(createdByChannelPartner)(parameters)
NX_REFLECTION_INSTRUMENT(SaasService, SaasService_fields)

/**%apidoc SaasState */
NX_REFLECTION_ENUM_CLASS(SaasState,
    /**%apidoc Saas services is not loaded */
    uninitialized,

    /**%apidoc Saas services is active */
    active,

    /**%apidoc Saas services are suspended manually */
    suspend,

    /**%apidoc Saas services are shutdown manually */
    shutdown,

    /**%apidoc Saas services are shutdown automatically */
    autoShutdown
);

/**%apidoc SaasPurshase */
struct NX_VMS_API SaasPurshase
{
    /**%apidoc Quantity of purchased service */
    int quantity = 0;

    bool operator==(const SaasPurshase&) const = default;
};
#define SaasPurshase_fields (quantity)
NX_REFLECTION_INSTRUMENT(SaasPurshase, SaasPurshase_fields)

/**%apidoc SaasState */
NX_REFLECTION_ENUM_CLASS(UseStatus,

    /**%apidoc Not initialized */
    uninitialized,

    /**%apidoc No issues */
    ok,

    /**%apidoc Service type should be blocked because of overuse.
     * The block date is in 'tmpExpirationDate' field
     */
    overUse
);

struct ServiceTypeStatus
{
    /**%apidoc Service status. If status is not 'ok'
     * services will be blocked at 'issueExpirationDate'.
     */
    UseStatus status;

    /**%apidoc There is issue with service overflow detected by license server.
     * The issue should be fixed till this date, otherwise VMS will start to
     * turn off services.
     */
    SaasDateTime issueExpirationDate;

    bool operator==(const ServiceTypeStatus&) const = default;
};
#define ServiceTypeState_fields (status)(issueExpirationDate)
NX_REFLECTION_INSTRUMENT(ServiceTypeStatus, ServiceTypeState_fields)

/**%apidoc Organization */
struct NX_VMS_API Organization
{
    /**%apidoc Organization id */
    QnUuid id;

    /**%apidoc Organization name */
    QString name;

    /**%apidoc Organization web page */
    QString webPage;

    bool operator==(const Organization&) const = default;
};
#define Organization_fields (id)(name)(webPage)
NX_REFLECTION_INSTRUMENT(Organization, Organization_fields)

/**%apidoc SaasSecurity */
struct NX_VMS_API SaasSecurity
{
    /**%apidoc Cloud licenses are periodically checked on the license server.
     * VMS server should send license usage reports to the license server using this period.
     */
    std::chrono::seconds checkPeriodS{};

    /**%apidoc Last license check date and time.
     * UTC time string in format 'YYYY-MM-DD hh:mm:ss'.
     */
    SaasDateTime lastCheck;

    /**%apidoc License is checked and available to use until this time.
     * The default value for prolongation is 30 check periods.
     * UTC time string in format 'YYYY-MM-DD hh:mm:ss'.
     */
    SaasDateTime tmpExpirationDate;

    std::map<QString, ServiceTypeStatus> status;

    bool operator==(const SaasSecurity&) const = default;
};
#define SaasSecurity_fields (checkPeriodS)(lastCheck)(tmpExpirationDate)(status)
NX_REFLECTION_INSTRUMENT(SaasSecurity, SaasSecurity_fields)

/**%apidoc SaasData from license server */
struct NX_VMS_API SaasData
{
    /**%apidoc Cloud system Id */
    QnUuid cloudSystemId;

    /**%apidoc Channel partner information */
    Organization channelPartner;

    /**%apidoc Organization information */
    Organization organization;

    /**%apidoc Saas state */
    SaasState state = SaasState::uninitialized;

    /**%apidoc The list of purchased services */
    std::map<QnUuid, SaasPurshase> services;

    /**%apidoc Security data. It is used to validate saas data*/
    SaasSecurity security;

    bool operator==(const SaasData&) const = default;
};
#define SaasData_fields (cloudSystemId)(channelPartner)(organization)(state)(services)(security)
NX_REFLECTION_INSTRUMENT(SaasData, SaasData_fields)

struct NX_VMS_API UpdateCloudLicensesRequest
{
    /**%apidoc Wait until request to the Licensing server is completed. */
    bool waitForDone = false;
};
#define UpdateCloudLicensesRequest_Fields (waitForDone)

NX_REFLECTION_INSTRUMENT(UpdateCloudLicensesRequest, UpdateCloudLicensesRequest_Fields)
NX_VMS_API_DECLARE_STRUCT_EX(UpdateCloudLicensesRequest, (json))

} // namespace nx::vms::api
