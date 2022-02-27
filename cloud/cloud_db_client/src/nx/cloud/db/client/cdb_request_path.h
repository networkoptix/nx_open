// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::cloud::db {

/**%apidoc GET /cdb/account/self
 * %return Account data. The account is found by credentials that were used to authorize the API call.
 *     %struct AccountData
 *
 * %apidoc DELETE /cdb/account/self
 * Delete account. The account to delete is found by credentials that were used to authorize the API call.
 * If the account owns at least one system, then this request fails with "403 Forbidden".
 * If the account has access to other systems, it is removed from those systems.
 */
static constexpr char kAccountPath[] = "/cdb/account/{email}";

static constexpr char kAccountRegisterPath[] = "/cdb/account/register";
static constexpr char kAccountActivatePath[] = "/cdb/account/activate";
static constexpr char kAccountGetPath[] = "/cdb/account/get";
static constexpr char kAccountUpdatePath[] = "/cdb/account/update";
static constexpr char kAccountPasswordResetPath[] = "/cdb/account/resetPassword";
static constexpr char kAccountReactivatePath[] = "/cdb/account/reactivate";
static constexpr char kAccountCreateTemporaryCredentialsPath[] = "/cdb/account/createTemporaryCredentials";

static constexpr char kAccountSecuritySettingsPath[] = "/cdb/account/{email}/settings/security";

static constexpr char kAccountForSharingPath[] = "/cdb/account/{email}/sharing-data";

static constexpr char kAccountEmailParam[] = "email";

//-------------------------------------------------------------------------------------------------

static constexpr char kSystemBindPath[] = "/cdb/system/bind";
static constexpr char kSystemUnbindPath[] = "/cdb/system/unbind";
static constexpr char kSystemGetPath[] = "/cdb/system/get";
static constexpr char kSystemSharePath[] = "/cdb/system/share";
static constexpr char kSystemGetCloudUsersPath[] = "/cdb/system/getCloudUsers";
static constexpr char kSystemGetAccessRoleListPath[] = "/cdb/system/getAccessRoleList";
static constexpr char kSystemRenamePath[] = "/cdb/system/rename";
static constexpr char kSystemUpdatePath[] = "/cdb/system/update";
static constexpr char kSystemRecordUserSessionStartPath[] = "/cdb/system/recordUserSessionStart";
static constexpr char kSystemHealthHistoryPath[] = "/cdb/system/healthHistory";
static constexpr char kSystemsMergedToASpecificSystem[] = "/cdb/system/{systemId}/merged_systems/";
static constexpr char kSystemsValidateMSSignature[] = "/cdb/system/{systemId}/signature/validate";

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

static constexpr char kSubscribeToSystemEventsPath[] = "/cdb/event/subscribe";

static constexpr char kPingPath[] = "/cdb/ping";

static constexpr char kEc2TransactionConnectionPathPrefix[] = "/cdb/ec2/";
static constexpr char kDeprecatedEc2TransactionConnectionPathPrefix[] = "/ec2/";

static constexpr char kVmsDbPrefix[] = "/cdb/vms-db";

static constexpr char kMaintenanceGetVmsConnections[] = "/cdb/maintenance/vmsConnections";
static constexpr char kMaintenanceGetTransactionLog[] = "/cdb/maintenance/transactionLog";
static constexpr char kMaintenanceGetStatistics[] = "/cdb/maintenance/statistics";

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
