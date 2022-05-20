// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::cloud::db {

// deprecated.
static constexpr char kAccountGetPath[] = "/cdb/account/get";
static constexpr char kAccountUpdatePath[] = "/cdb/account/update";
// end of deprecated.

static constexpr char kAccountRegisterPath[] = "/cdb/account/register";
static constexpr char kAccountActivatePath[] = "/cdb/account/activate";
static constexpr char kAccountPasswordResetPath[] = "/cdb/account/resetPassword";
static constexpr char kAccountReactivatePath[] = "/cdb/account/reactivate";
static constexpr char kAccountCreateTemporaryCredentialsPath[] = "/cdb/account/createTemporaryCredentials";

static constexpr char kAccountPath[] = "/cdb/account/{email}";
static constexpr char kAccountSelfPath[] = "/cdb/account/self";

static constexpr char kAccountSecuritySettingsPath[] = "/cdb/account/{email}/settings/security";
static constexpr char kAccountForSharingPath[] = "/cdb/account/{email}/sharing-data";

static constexpr char kAccountEmailParam[] = "email";

//-------------------------------------------------------------------------------------------------

// deprecated.
static constexpr char kSystemUnbindPath[] = "/cdb/system/unbind";
static constexpr char kSystemGetPath[] = "/cdb/system/get";
static constexpr char kSystemRenamePath[] = "/cdb/system/rename";
static constexpr char kSystemUpdatePath[] = "/cdb/system/update";
static constexpr char kSystemGetCloudUsersPath[] = "/cdb/system/getCloudUsers";
// end of deprecated.

static constexpr char kSystemBindPath[] = "/cdb/system/bind";
static constexpr char kSystemSharePath[] = "/cdb/system/share";
static constexpr char kSystemGetAccessRoleListPath[] = "/cdb/system/getAccessRoleList";
static constexpr char kSystemRecordUserSessionStartPath[] = "/cdb/system/recordUserSessionStart";

static constexpr char kSystemsPath[] = "/cdb/system/";

static constexpr char kSystemPath[] = "/cdb/system/{systemId}";
static constexpr char kSystemHealthHistoryPath[] = "/cdb/system/{systemId}/health-history";
static constexpr char kSystemsMergedToASpecificSystem[] = "/cdb/system/{systemId}/merged_systems/";
static constexpr char kSystemsValidateMSSignature[] = "/cdb/system/{systemId}/signature/validate";
static constexpr char kSystemUsersPath[] = "/cdb/system/{systemId}/users";
static constexpr char kSystemUserPath[] = "/cdb/system/{systemId}/users/{email}";

static constexpr char kAuthGetNoncePath[] = "/cdb/auth/getNonce";
static constexpr char kAuthGetAuthenticationPath[] = "/cdb/auth/getAuthentication";
static constexpr char kAuthResolveUserCredentials[] = "/cdb/auth_provider/caller-identity";
static constexpr char kAuthResolveUserCredentialsList[] = "/cdb/auth_provider/caller-identity/list";
static constexpr char kAuthSystemAccessLevel[] = "/cdb/auth_provider/system/{systemId}/access-level";
static constexpr char kAuthVmsServerCertificatePublicKey[] =
    "/cdb/auth_provider/system/{systemId}/server/{serverId}/certificate/{fingerprint}/public-key";

static constexpr char kOauthTokenPath[] = "/cdb/oauth2/token";
static constexpr char kOauthTokenValidatePath[] = "/cdb/oauth2/token/{token}";
static constexpr char kOauthTokensDeletePath[] = "/cdb/oauth2/user/self/client/{clientId}";
static constexpr char kOauthLogoutPath[] = "/cdb/oauth2/user/self";
static constexpr char kTokenParam[] = "token";
static constexpr char kClientIdParam[] = "clientId";

static constexpr char kTwoFactorAuthGetKey[] = "/cdb/account/self/2fa/totp/key";
static constexpr char kTwoFactorAuthValidateKey[] = "/cdb/account/self/2fa/totp/key/{key}";
static constexpr char kTwoFactorAuthBackupCodes[] = "/cdb/account/self/2fa/backup-code/";
static constexpr char kTwoFactorAuthBackupCode[] = "/cdb/account/self/2fa/backup-code/{code}";
static constexpr char kKeyParam[] = "key";
static constexpr char kCodeParam[] = "code";

//-------------------------------------------------------------------------------------------------

static constexpr char kSystemOwnershipOffers[] = "/cdb/offered-systems";
static constexpr char kSystemOwnershipOffer[] = "/cdb/offered-systems/{systemId}";

//-------------------------------------------------------------------------------------------------

static constexpr char kPingPath[] = "/cdb/ping";

static constexpr char kEc2TransactionConnectionPathPrefix[] = "/cdb/ec2/";
static constexpr char kDeprecatedEc2TransactionConnectionPathPrefix[] = "/ec2/";

static constexpr char kVmsDbPrefix[] = "/cdb/vms-db";

static constexpr char kMaintenanceGetVmsConnections[] = "/cdb/maintenance/vmsConnections";
static constexpr char kMaintenanceGetTransactionLog[] = "/cdb/maintenance/transactionLog";
static constexpr char kMaintenanceGetStatistics[] = "/cdb/maintenance/statistics";
static constexpr char kMaintenanceGetSettings[] = "/cdb/maintenance/settings";

static constexpr char kStatisticsPath[] = "/cdb/statistics";
static constexpr char kStatisticsMetricsPath[] = "/cdb/statistics/metrics/";

static constexpr char kDeprecatedCloudModuleXmlPath[] = "/cdb/cloud_modules.xml";
static constexpr char kAnotherDeprecatedCloudModuleXmlPath[] = "/api/cloud_modules.xml";

static constexpr char kDiscoveryCloudModuleXmlPath[] = "/discovery/v1/cloud_modules.xml";

static constexpr char kApiPrefix[] = "/cdb";

static constexpr char kApiDocPrefix[] = "/cdb/docs/api";

static constexpr char kSystemIdParam[] = "systemId";
static constexpr char kServerIdParam[] = "serverId";
static constexpr char kFingerprintParam[] = "fingerprint";
static constexpr char kIsValidParam[] = "valid";

static constexpr char kObjectLocationPrefix[] = "/cdb/objectLocation";

} // namespace nx::cloud::db
