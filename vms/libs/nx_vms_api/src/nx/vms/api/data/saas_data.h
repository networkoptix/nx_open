// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <variant>

#include <QtCore/QDateTime>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/latin1_array.h>
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
    nx::Uuid id;

    /**%apidoc service type. Service parameters depends on service type. */
    QString type;

    /**%apidoc Service state (active or obsolete). */
    SaasServiceState state = SaasServiceState::active;

    /**%apidoc Human readable name of service. */
    QString displayName;

    /**%apidoc Description of service for Channel partner's users. */
    QString description;

    /**%apidoc Identifier of Brand owner or Channel partner issuing the service. */
    nx::Uuid createdByChannelPartner;

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
    suspended,

    /**%apidoc Saas services are shut down manually */
    shutdown,

    /**%apidoc Saas services are shut down automatically */
    autoShutdown
);

/**%apidoc SaasPurshase */
struct NX_VMS_API SaasPurshase
{
    /**%apidoc Usage count of purchased service */
    int used = 0;

    /**%apidoc Quantity of purchased service */
    int quantity = 0;

    bool operator==(const SaasPurshase&) const = default;
};
#define SaasPurshase_fields (used)(quantity)
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

/**%apidoc ChannelPartnerSupportEmail */
struct NX_VMS_API ChannelPartnerSupportContactInfoRecord
{
    /**%apidoc Email address, phone number or web page address */
    QString value;

    /**%apidoc[opt] Description of the contact information record */
    QString description;

    bool operator==(const ChannelPartnerSupportContactInfoRecord&) const = default;
};
NX_REFLECTION_INSTRUMENT(ChannelPartnerSupportContactInfoRecord, (value)(description))

/**%apidoc ChannelPartnerSupportCustomInfo */
struct NX_VMS_API ChannelPartnerSupportCustomInfo
{
    /**%apidoc The title under which provided information will be placed */
    QString label;

    /**%apidoc Any information in the text form, e.g. street address */
    QString value;

    bool operator==(const ChannelPartnerSupportCustomInfo&) const = default;
};
NX_REFLECTION_INSTRUMENT(ChannelPartnerSupportCustomInfo, (label)(value))

/**%apidoc ChannelPartnerSupportInformation */
struct NX_VMS_API ChannelPartnerSupportInformation
{
    /**%apidoc Set of structures describing web page addresses */
    std::vector<ChannelPartnerSupportContactInfoRecord> sites;

    /**%apidoc Set of structures describing phone numbers */
    std::vector<ChannelPartnerSupportContactInfoRecord> phones;

    /**%apidoc Set of structures describing email addresses */
    std::vector<ChannelPartnerSupportContactInfoRecord> emails;

    /**%apidoc Set of structures describing any other information */
    std::vector<ChannelPartnerSupportCustomInfo> custom;

    bool operator==(const ChannelPartnerSupportInformation&) const = default;
};
#define ChannelPartnerSupportInformation_fields (sites)(phones)(emails)(custom)
NX_REFLECTION_INSTRUMENT(ChannelPartnerSupportInformation, ChannelPartnerSupportInformation_fields)

/**%apidoc ChannelPartner */
struct NX_VMS_API ChannelPartner
{
    /**%apidoc Channel partner id */
    nx::Uuid id;

    /**%apidoc Channel partner name */
    QString name;

    /**%apidoc Channel partner support information, addresses, pnone numbers etc. */
    ChannelPartnerSupportInformation supportInformation;

    bool operator==(const ChannelPartner&) const = default;
};
#define ChannelPartner_fields (id)(name)(supportInformation)
NX_REFLECTION_INSTRUMENT(ChannelPartner, ChannelPartner_fields)

/**%apidoc Organization */
struct NX_VMS_API Organization
{
    /**%apidoc Organization id */
    nx::Uuid id;

    /**%apidoc Organization name */
    QString name;

    bool operator==(const Organization&) const = default;
};
#define Organization_fields (id)(name)
NX_REFLECTION_INSTRUMENT(Organization, Organization_fields)

/**%apidoc SaasSecurity */
struct NX_VMS_API SaasSecurity
{
    /**%apidoc Data are periodically checked on the Channel Partner Server.
     * VMS server should send usage reports to the Channel Partner Server using this period.
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
    /**%apidoc Cloud Site Id */
    nx::Uuid cloudSystemId;

    /**%apidoc Channel partner information */
    ChannelPartner channelPartner;

    /**%apidoc Organization information */
    Organization organization;

    /**%apidoc Saas state */
    SaasState state = SaasState::uninitialized;

    /**%apidoc The list of purchased services */
    std::map<nx::Uuid, SaasPurshase> services;

    /**%apidoc Security data. It is used to validate saas data*/
    SaasSecurity security;

    /**%apidoc Data block signature. Signature is calculated for data with empty signature field.*/
    QnLatin1Array signature;

    bool operator==(const SaasData&) const = default;
};
#define SaasData_fields (cloudSystemId)(channelPartner)(organization)(state)(services)(security)(signature)
NX_REFLECTION_INSTRUMENT(SaasData, SaasData_fields)

} // namespace nx::vms::api
