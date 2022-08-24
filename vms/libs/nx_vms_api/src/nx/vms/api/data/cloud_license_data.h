// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>

#include <QtCore/QDateTime>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/software_version.h>
#include <nx/utils/uuid.h>

#include "data_macros.h"

namespace nx::vms::api {

/**
 * Basic information about the license order. 
 */
struct NX_VMS_API CloudLicenseOrderParams
{
    /**%apidoc Unique license key. */
    QString licenseKey;

    /**%apidoc Product brand to which this license applies. */
    QString brand;

    /**%apidoc Duration of the license in human-readable format. Example: "1Y", "3M" */
    QString licensePeriod;

    /**%apidoc Total count of allowed deactivations for the license. */
    int totalDeactivationNumber = 0;
};
#define CloudLicenseOrderParams_fields (licenseKey)(brand)(licensePeriod)(totalDeactivationNumber)

/**
 * Information which is filled when the license is generated.
 */
struct NX_VMS_API CloudLicenseParams
{
    using ServiceParameters = std::map<QString, QString>;

    /**%apidoc List of services license is granted for (`localRecording`, `cloudStorage`, e.t.c).
     * Each service can have number of it's own parameters, e.g. 'localRecording' service can
     * have 'totalChannelNumber' parameter.
     */
    std::map<QString, ServiceParameters> services;

    /**%apidoc Parameters of the license order. */
    CloudLicenseOrderParams orderParams;
    
    /**%apidoc Reserved for the future use. */
    QString masterSignature; 
};
#define CloudLicenseParams_fields (services)(orderParams)(masterSignature)

/**
 * The "state" contains the main information that depends to the current license state
 */
struct NX_VMS_API CloudLicenseState
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS (State,
        active,
        inactive
    );

    /**%apidoc License state (active or inactive). */
    State licenseState = State::active;

    /**%apidoc SystemId of the license. It is null unless license is activated. */
    QnUuid cloudSystemId;

    /**%apidoc Date when the license was activated for the first time.
     * UTC time string in format 'YYYY-MM-DD hh:mm:ss'.
     */
    QString firstActivationDate;

    /**%apidoc The latest date when the license was activated.
     * UTC time string in format 'YYYY-MM-DD hh:mm:ss'.
     */
    QString lastActivationDate;
    
    /**%apidoc License expiration time. Empty if license was never activated.
     * UTC time string in format 'YYYY-MM-DD hh:mm:ss'.
     */
    QString expirationDate;

    /**%apidoc The total number of available deactivation for this license. */
    int deactivationsRemaining = 0;
};
#define CloudLicenseState_fields (licenseState)(cloudSystemId)(firstActivationDate)(lastActivationDate)(expirationDate)(deactivationsRemaining)

/**
 * Security-related block for the single license.
 */
struct NX_VMS_API CloudLicenseSecurity
{
    /**%apidoc Cloud licenses are periodically checked on the license server. 
     * VMS server should send license usage reports to the license server using this period.
     */
    std::chrono::seconds checkPeriodS{};

    /**%apidoc Last license check date and time. 
     * UTC time string in format 'YYYY-MM-DD hh:mm:ss'. 
     */
    QString lastCheck;

    /**%apidoc License is checked and available to use until this time. 
     * The default value for prolongation is 30 check periods. 
     * UTC time string in format 'YYYY-MM-DD hh:mm:ss'. 
     */
    QString tmpExpirationDate;

    /**%apidoc Empty string in case of there are no security issues.
     * Otherwise error message from the license server with description why the license was blocked.
     */
    QString issue;
};
#define CloudLicenseSecurity_fields (checkPeriodS)(lastCheck)(tmpExpirationDate)(issue)

struct NX_VMS_API CloudLicenseData
{
    static QDateTime toDateTime(const QString& value);

    nx::utils::SoftwareVersion version;
    CloudLicenseParams params;
    CloudLicenseState state;
    CloudLicenseSecurity security;
    QString signature;
};
#define CloudLicenseData_fields (version)(params)(state)(security)(signature)

struct NX_VMS_API UpdateCloudLicensesRequest
{
    /**%apidoc Wait until request to the Licensing server is completed. */
    bool waitForDone = false;
};
#define UpdateCloudLicensesRequest_Fields (waitForDone)

struct NX_VMS_API ActivateCloudLicensesRequest
{
    QString licenseKey;
    QString cloudSystemId;
};
#define ActivateCloudLicensesRequest_Fields (licenseKey)(cloudSystemId)

NX_REFLECTION_INSTRUMENT(CloudLicenseOrderParams, CloudLicenseOrderParams_fields)
NX_REFLECTION_INSTRUMENT(CloudLicenseParams, CloudLicenseParams_fields)
NX_REFLECTION_INSTRUMENT(CloudLicenseState, CloudLicenseState_fields)
NX_REFLECTION_INSTRUMENT(CloudLicenseSecurity, CloudLicenseSecurity_fields)
NX_REFLECTION_INSTRUMENT(CloudLicenseData, CloudLicenseData_fields)
NX_REFLECTION_INSTRUMENT(UpdateCloudLicensesRequest, UpdateCloudLicensesRequest_Fields)
NX_REFLECTION_INSTRUMENT(ActivateCloudLicensesRequest, ActivateCloudLicensesRequest_Fields)

NX_VMS_API_DECLARE_STRUCT_EX(UpdateCloudLicensesRequest, (json))

} // namespace nx::vms::api
