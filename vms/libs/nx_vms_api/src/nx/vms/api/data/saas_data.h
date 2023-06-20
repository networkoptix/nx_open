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

class SaasDateTime: public QDateTime
{
public:
    SaasDateTime() = default;
    SaasDateTime(const QDateTime& value): QDateTime(value) {}
};

bool NX_VMS_API fromString(const std::string& value, SaasDateTime* target);

/**%apidoc Saas service state */
NX_REFLECTION_ENUM_CLASS(SaasServiceState,
    /**%apidoc active */
    active,
    
    /**%apidoc disabled */
    obsolete
);

using SaasServiceParameters = std::map<std::string, std::variant<std::string, int>>;

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
    int totalChannelNumber = 0;

    /**%apidoc Archive duration in days */
    int days = 0;

    /**%apidoc Maxumum camera resolution in megapixels */
    int maxResolution = kUnlimitedResolution;

    static SaasCloudStorageParameters fromParams(const SaasServiceParameters& parameters);
};
#define SaasCloudStorageParameters_fields (totalChannelNumber)(days)(maxResolution)
NX_REFLECTION_INSTRUMENT(SaasCloudStorageParameters, SaasCloudStorageParameters_fields)

/**%apidoc Saas service parameters for 'analytics' service type */
struct NX_VMS_API SaasAnalyticsParamters
{
    /**%apidoc Amount of channels */
    int totalChannelNumber = 0;

    /**%apidoc Guid of analytics engine Id */
    QnUuid integrationId;

    static SaasAnalyticsParamters fromParams(const SaasServiceParameters& parameters);
};
#define SaasAnalyticsParamters_fields (totalChannelNumber)(integrationId)
NX_REFLECTION_INSTRUMENT(SaasAnalyticsParamters, SaasAnalyticsParamters_fields)

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
};
#define SaasService_fields (id)(type)(state)(displayName)(description)(createdByChannelPartner)(parameters)
NX_REFLECTION_INSTRUMENT(SaasService, SaasService_fields)

/**%apidoc SaasState */
NX_REFLECTION_ENUM_CLASS(SaasState,
    /**%apidoc Saas services is active */
    active,

    /**%apidoc Saas services are suspended manually */
    suspend,

    /**%apidoc Saas services are suspended automatically */
    shutdown
);

/**%apidoc SaasPurshase */
struct NX_VMS_API SaasPurshase
{
    /**%apidoc Quantity of purchased service */
    int quantity = 0;
};
#define SaasPurshase_fields (quantity)
NX_REFLECTION_INSTRUMENT(SaasPurshase, SaasPurshase_fields)

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

    /**%apidoc Empty string in case of there are no security issues.
     * Otherwise error message from the license server with description why the license was blocked.
     */
    QString issue;
};
#define SaasSecurity_fields (checkPeriodS)(lastCheck)(tmpExpirationDate)(issue)
NX_REFLECTION_INSTRUMENT(SaasSecurity, SaasSecurity_fields)

/**%apidoc SaasData from license server */
struct NX_VMS_API SaasData
{
    /**%apidoc Cloud system Id */
    QnUuid cloudSystemId;

    /**%apidoc Saas state */
    SaasState state = SaasState::active;

    /**%apidoc The list of purchased services */
    std::map<QnUuid, SaasPurshase> services;

    /**%apidoc Security data. It is used to validate saas data*/
    SaasSecurity security;
};
#define SaasData_fields (cloudSystemId)(state)(services)(security)
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
