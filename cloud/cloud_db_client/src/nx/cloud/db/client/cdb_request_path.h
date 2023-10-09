// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::cloud::db {

static constexpr char kAccountRegisterPath[] = "/cdb/account/register";
static constexpr char kAccountActivatePath[] = "/cdb/account/activate";
static constexpr char kAccountPasswordResetPath[] = "/cdb/account/resetPassword";
static constexpr char kAccountReactivatePath[] = "/cdb/account/reactivate";
static constexpr char kAccountCreateTemporaryCredentialsPath[] = "/cdb/account/createTemporaryCredentials";

static constexpr char kAccountPath[] = "/cdb/account/{accountEmail}";
static constexpr char kAccountSelfPath[] = "/cdb/account/self";
static constexpr char kAccountStatusPath[] = "/cdb/account/{accountEmail}/status";

static constexpr char kAccountSecuritySettingsPath[] = "/cdb/account/{accountEmail}/settings/security";
static constexpr char kAccountForSharingPath[] = "/cdb/account/{accountEmail}/sharing-data";

static constexpr char kAccountEmailParam[] = "accountEmail";

//-------------------------------------------------------------------------------------------------

static constexpr char kSystemBindPath[] = "/cdb/systems/bind";
static constexpr char kSystemSharePath[] = "/cdb/systems/share";
static constexpr char kSystemGetAccessRoleListPath[] = "/cdb/systems/getAccessRoleList";
static constexpr char kSystemRecordUserSessionStartPath[] = "/cdb/systems/recordUserSessionStart";

static constexpr char kSystemsPath[] = "/cdb/systems";

static constexpr char kSystemPath[] = "/cdb/systems/{systemId}";
static constexpr char kSystemHealthHistoryPath[] = "/cdb/systems/{systemId}/health-history";
static constexpr char kSystemsMergedToASpecificSystem[] = "/cdb/systems/{systemId}/merged_systems/";
static constexpr char kSystemsValidateMSSignature[] = "/cdb/systems/{systemId}/signature/validate";
static constexpr char kSystemUsersPath[] = "/cdb/systems/{systemId}/users";
static constexpr char kSystemUserPath[] = "/cdb/systems/{systemId}/users/{accountEmail}";

static constexpr char kSystemAttributesPath[] = "/cdb/systems/{systemId}/attributes";
static constexpr char kSystemAttributePath[] = "/cdb/systems/{systemId}/attributes/{attributeName}";
static constexpr char kSystemUserAttributesPath[] = "/cdb/systems/{systemId}/users/{accountEmail}/attributes";
static constexpr char kSystemUserAttributePath[] = "/cdb/systems/{systemId}/users/{accountEmail}/attributes/{attributeName}";
static constexpr char kSystemAttributeParam[] = "attributeName";

static constexpr char kSendNotificationQueryParam[] = "sendNotification";

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
static constexpr char kOauthStunTokenPath[] = "/cdb/oauth2/stun-token";
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

static constexpr char kSystemUsersBatchPath[] = "/cdb/systems/users/batch";
static constexpr char kSystemUsersBatchStatePath[] = "/cdb/systems/users/batch/{batchId}/state";
static constexpr char kSystemUsersBatchErrorInfoPath[] = "/cdb/systems/users/batch/{batchId}/error";
static constexpr char kBatchIdParam[] = "batchId";

//-------------------------------------------------------------------------------------------------
// Organizations.

static constexpr char kOrganizationSystemsPath[] = "/cdb/organizations/{organizationId}/systems";

static constexpr char kOrganizationSystemPath[] =
    "/cdb/organizations/{organizationId}/systems/{systemId}";

static constexpr char kOrganizationSystemUsersPath[] =
    "/cdb/organizations/{organizationId}/systems/{systemId}/users";

static constexpr char kOrganizationIdParam[] = "organizationId";

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

static constexpr char kPublicApiDocPrefix[] = "/cdb/docs/api";
static constexpr char kInternalApiDocPrefix[] = "/cdb/docs/internal-api";

static constexpr char kSystemIdParam[] = "systemId";
static constexpr char kServerIdParam[] = "serverId";
static constexpr char kFingerprintParam[] = "fingerprint";
static constexpr char kIsValidParam[] = "valid";

static constexpr char kObjectLocationPrefix[] = "/cdb/objectLocation";

//-------------------------------------------------------------------------------------------------

namespace deprecated {

static constexpr char kAccountGetPath[] = "/cdb/account/get";
static constexpr char kAccountUpdatePath[] = "/cdb/account/update";

static constexpr char kSystemUnbindPath[] = "/cdb/systems/unbind";
static constexpr char kSystemGetPath[] = "/cdb/systems/get";
static constexpr char kSystemRenamePath[] = "/cdb/systems/rename";
static constexpr char kSystemUpdatePath[] = "/cdb/systems/update";
static constexpr char kSystemGetCloudUsersPath[] = "/cdb/systems/getCloudUsers";

} // namespace deprecated

} // namespace nx::cloud::db
