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

NX_REFLECTION_ENUM_CLASS(SaasServiceState,
    /**%apidoc active */
    active,

    /**%apidoc disabled */
    obsolete
);

using SaasServiceParameters = std::map<std::string, std::variant<QString, int>>;

/**%apidoc SaaS service parameters for 'localRecording' service type. */
struct NX_VMS_API SaasLocalRecordingParameters
{
    /**%apidoc Amount of channels. */
    int totalChannelNumber = 1;

    static SaasLocalRecordingParameters fromParams(const SaasServiceParameters& parameters);
};
#define SaasLocalRecordingParameters_Fields (totalChannelNumber)
NX_REFLECTION_INSTRUMENT(SaasLocalRecordingParameters, SaasLocalRecordingParameters_Fields)
QN_FUSION_DECLARE_FUNCTIONS(SaasLocalRecordingParameters, (json), NX_VMS_API)

/**%apidoc Saas service parameters for 'cloudStorage' service type. */
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
#define SaasCloudStorageParameters_Fields (totalChannelNumber)(days)(maxResolutionMp)
NX_REFLECTION_INSTRUMENT(SaasCloudStorageParameters, SaasCloudStorageParameters_Fields)
QN_FUSION_DECLARE_FUNCTIONS(SaasCloudStorageParameters, (json), NX_VMS_API)

/**%apidoc Saas service parameters for 'analytics' service type. */
struct NX_VMS_API SaasAnalyticsParameters
{
    int totalChannelNumber = 1;

    /**%apidoc Analytics Plugin id. */
    QString integrationId;

    static SaasAnalyticsParameters fromParams(const SaasServiceParameters& parameters);
};
#define SaasAnalyticsParameters_Fields (totalChannelNumber)(integrationId)
NX_REFLECTION_INSTRUMENT(SaasAnalyticsParameters, SaasAnalyticsParameters_Fields)
QN_FUSION_DECLARE_FUNCTIONS(SaasAnalyticsParameters, (json), NX_VMS_API)

struct NX_VMS_API SaasService
{
    static const QString kLocalRecordingServiceType;
    static const QString kAnalyticsIntegrationServiceType;
    static const QString kCloudRecordingType;

    /**%apidoc Internal service identifier. */
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
    std::map<std::string, std::variant<QString, int>> parameters;

    bool operator==(const SaasService&) const = default;
};
#define SaasService_Fields (id)(type)(state)(displayName)(description)(createdByChannelPartner)(parameters)
NX_REFLECTION_INSTRUMENT(SaasService, SaasService_Fields)
QN_FUSION_DECLARE_FUNCTIONS(SaasService, (json), NX_VMS_API)

NX_REFLECTION_ENUM_CLASS(SaasState,
    /**%apidoc Saas services is not loaded. */
    uninitialized,

    /**%apidoc Saas services is active. */
    active,

    /**%apidoc Saas services are suspended manually. */
    suspended,

    /**%apidoc Saas services are shut down manually. */
    shutdown,

    /**%apidoc Saas services are shut down automatically. */
    autoShutdown
);

struct NX_VMS_API SaasPurchase
{
    /**%apidoc Usage count of purchased service. */
    int used = 0;

    /**%apidoc Quantity of purchased service. */
    int quantity = 0;

    bool operator==(const SaasPurchase&) const = default;
};
#define SaasPurchase_Fields (used)(quantity)
NX_REFLECTION_INSTRUMENT(SaasPurchase, SaasPurchase_Fields)
QN_FUSION_DECLARE_FUNCTIONS(SaasPurchase, (json), NX_VMS_API)

NX_REFLECTION_ENUM_CLASS(UseStatus,

    /**%apidoc Not initialized. */
    uninitialized,

    /**%apidoc No issues. */
    ok,

    /**%apidoc Service type should be blocked because of overuse.
     * The block date is in 'tmpExpirationDate' field.
     */
    overUse
);

struct ServiceTypeStatus
{
    /**%apidoc Service status. If status is not 'ok'
     * services will be blocked at 'issueExpirationDate'.
     */
    UseStatus status;

    /**%apidoc:string There is issue with service overflow detected by license server.
     * The issue should be fixed till this date, otherwise VMS will start to
     * turn off services.
     */
    SaasDateTime issueExpirationDate;

    bool operator==(const ServiceTypeStatus&) const = default;
};
#define ServiceTypeStatus_Fields (status)(issueExpirationDate)
NX_REFLECTION_INSTRUMENT(ServiceTypeStatus, ServiceTypeStatus_Fields)
QN_FUSION_DECLARE_FUNCTIONS(ServiceTypeStatus, (json), NX_VMS_API)

struct NX_VMS_API ChannelPartnerSupportContactInfoRecord
{
    /**%apidoc Email address, phone number or web page address. */
    QString value;

    /**%apidoc[opt] Description of the contact information record. */
    QString description;

    bool operator==(const ChannelPartnerSupportContactInfoRecord&) const = default;
};
#define ChannelPartnerSupportContactInfoRecord_Fields (value)(description)
NX_REFLECTION_INSTRUMENT(ChannelPartnerSupportContactInfoRecord, ChannelPartnerSupportContactInfoRecord_Fields)
QN_FUSION_DECLARE_FUNCTIONS(ChannelPartnerSupportContactInfoRecord, (json), NX_VMS_API)

struct NX_VMS_API ChannelPartnerSupportCustomInfo
{
    /**%apidoc The title under which provided information will be placed. */
    QString label;

    /**%apidoc Any information in the text form, e.g. street address. */
    QString value;

    bool operator==(const ChannelPartnerSupportCustomInfo&) const = default;
};
#define ChannelPartnerSupportCustomInfo_Fields (label)(value)
NX_REFLECTION_INSTRUMENT(ChannelPartnerSupportCustomInfo, ChannelPartnerSupportCustomInfo_Fields)
QN_FUSION_DECLARE_FUNCTIONS(ChannelPartnerSupportCustomInfo, (json), NX_VMS_API)

struct NX_VMS_API ChannelPartnerSupportInformation
{
    /**%apidoc Set of structures describing web page addresses. */
    std::vector<ChannelPartnerSupportContactInfoRecord> sites;

    /**%apidoc Set of structures describing phone numbers. */
    std::vector<ChannelPartnerSupportContactInfoRecord> phones;

    /**%apidoc Set of structures describing email addresses. */
    std::vector<ChannelPartnerSupportContactInfoRecord> emails;

    /**%apidoc Set of structures describing any other information. */
    std::vector<ChannelPartnerSupportCustomInfo> custom;

    bool operator==(const ChannelPartnerSupportInformation&) const = default;
};
#define ChannelPartnerSupportInformation_Fields (sites)(phones)(emails)(custom)
NX_REFLECTION_INSTRUMENT(ChannelPartnerSupportInformation, ChannelPartnerSupportInformation_Fields)
QN_FUSION_DECLARE_FUNCTIONS(ChannelPartnerSupportInformation, (json), NX_VMS_API)

struct NX_VMS_API ChannelPartner
{
    nx::Uuid id;
    QString name;

    /**%apidoc Channel partner support information, addresses, pnone numbers etc. */
    ChannelPartnerSupportInformation supportInformation;

    bool operator==(const ChannelPartner&) const = default;
};
#define ChannelPartner_Fields (id)(name)(supportInformation)
NX_REFLECTION_INSTRUMENT(ChannelPartner, ChannelPartner_Fields)
QN_FUSION_DECLARE_FUNCTIONS(ChannelPartner, (json), NX_VMS_API)

struct NX_VMS_API Organization
{
    nx::Uuid id;
    QString name;

    bool operator==(const Organization&) const = default;
};
#define Organization_Fields (id)(name)
NX_REFLECTION_INSTRUMENT(Organization, Organization_Fields)
QN_FUSION_DECLARE_FUNCTIONS(Organization, (json), NX_VMS_API)

struct NX_VMS_API SaasSecurity
{
    /**%apidoc Data are periodically checked on the Channel Partner Server.
     * VMS server should send usage reports to the Channel Partner Server using this period.
     */
    std::chrono::seconds checkPeriodS{};

    /**%apidoc:string Last license check date and time.
     * UTC time string in format 'YYYY-MM-DD hh:mm:ss'.
     */
    SaasDateTime lastCheck;

    /**%apidoc:string License is checked and available to use until this time.
     * The default value for prolongation is 30 check periods.
     * UTC time string in format 'YYYY-MM-DD hh:mm:ss'.
     */
    SaasDateTime tmpExpirationDate;

    std::map<QString, ServiceTypeStatus> status;

    bool operator==(const SaasSecurity&) const = default;
};
#define SaasSecurity_Fields (checkPeriodS)(lastCheck)(tmpExpirationDate)(status)
NX_REFLECTION_INSTRUMENT(SaasSecurity, SaasSecurity_Fields)
QN_FUSION_DECLARE_FUNCTIONS(SaasSecurity, (json), NX_VMS_API)

/**%apidoc SaasData from license server. */
struct NX_VMS_API SaasData
{
    /**%apidoc Cloud Site Id */
    nx::Uuid cloudSystemId;
    ChannelPartner channelPartner;
    Organization organization;
    SaasState state = SaasState::uninitialized;

    /**%apidoc The list of purchased services. */
    std::map<nx::Uuid, SaasPurchase> services;

    /**%apidoc Security data. It is used to validate saas data. */
    SaasSecurity security;

    /**%apidoc Data block signature. Signature is calculated for data with empty signature field. */
    QnLatin1Array signature;

    bool operator==(const SaasData&) const = default;
};
#define SaasData_Fields (cloudSystemId)(channelPartner)(organization)(state)(services)(security)(signature)
NX_REFLECTION_INSTRUMENT(SaasData, SaasData_Fields)
QN_FUSION_DECLARE_FUNCTIONS(SaasData, (json), NX_VMS_API)

struct NX_VMS_API SaasWithServices: SaasData
{
    /**%apidoc SaaS services available to the Site from Channel Partners, indexed by the service ID. */
    std::map<nx::Uuid, nx::vms::api::SaasService> servicesAvailable;
};
#define SaasWithServices_Fields SaasData_Fields(servicesAvailable)
NX_REFLECTION_INSTRUMENT(SaasWithServices, SaasWithServices_Fields)
QN_FUSION_DECLARE_FUNCTIONS(SaasWithServices, (json), NX_VMS_API)

} // namespace nx::vms::api
