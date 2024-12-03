// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**@file
 * Intended to be included into the Server code.
 */

static void registerLoginRestHandlers(
    nx::network::rest::HandlerPool* processorPool,
    auth::Authenticator* authenticator)
{
    const auto reg = processorPool->createRegisterHandlerFunctor();

    /**%apidoc GET /rest/v{1-}/login/users/{username}
     * Retrieves the User type and supported login options for the specified User.
     * %caption Get User login info
     * %ingroup Login
     * %param:string username
     *     %example admin
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_with
     * %param[ref] _local
     * %permissions Authorization is not required.
     * %return:{LoginUser}
     *
     **%apidoc GET /rest/v{1-}/login/users
     * Retrieves the User type and supported login options for all Users in the Site.
     * %caption Get all Users' login info
     * %ingroup Login
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Administrator with a fresh session.
     * %return:{LoginUserList}
     */
    reg("rest/v{1-}/login/users/:username?", GlobalPermission::none,
        std::make_unique<LoginUsersHandler>(authenticator));

    /**%apidoc POST /rest/v{1-}/login/sessions
     * Logs in to the Site and generates a session token for further request authorization.
     * %caption Create login Session
     * %ingroup Login
     * %struct LoginSessionRequest
     * %param[ref] _strict
     * %permissions Authorization is not required.
     * %return:{LoginSession}
     *
     **%apidoc GET /rest/v{1-}/login/sessions/{token}
     * Retrieves the session information for the specified token.
     * %caption Get login Session
     * %ingroup Login
     * %struct LoginSessionFilter
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_with
     * %param[ref] _local
     * %permissions Authorization is not required.
     * %return:{LoginSession}
     *
     **%apidoc GET /rest/v{1-}/login/sessions
     * Retrieves the session information about all sessions known to this Server. The session
     * information from the remote sources (other Servers or the Cloud) may be inaccurate and/or
     * incomplete.
     * %caption Get all login Sessions
     * %ingroup Login
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Administrator with a fresh session.
     * %return:{LoginSessionList}
     *
     **%apidoc DELETE /rest/v{1-}/login/sessions/{token}
     * Terminates the session for the specified token.
     * %caption Delete login Session
     * %ingroup Login
     * %param:string token If the value "current" is used, the token is obtained from the
     *     authorization.
     *     %example current
     * %param[ref] _local
     * %param[ref] _strict
     * %permissions Authorization is not required.
     */
    reg("rest/v{1-}/login/sessions/:token?", GlobalPermission::none,
        std::make_unique<LoginSessionsHandler>(authenticator));

    /**%apidoc POST /rest/v{3-}/login/temporaryToken
     * Logs in the Temporary User to the Site and generates a Session Token for further
     * request authorization.
     * %caption Log in Temporary User
     * %ingroup Login
     * %struct TemporaryLoginSessionRequest
     * %param[ref] _strict
     * %permissions Authorization is not required.
     * %return:{LoginSession}
     */
    reg("rest/v{3-}/login/temporaryToken", GlobalPermission::none,
        std::make_unique<TemporaryUserLoginHandler>(authenticator));

    /**%apidoc POST /rest/v{3-}/login/tickets
     * Generates a Ticket that can be used for a single request authorization.
     * The Ticket is valid only for a request to the Server that generated it
     * and can't be used to access other Servers.
     * %caption Create authorization Ticket
     * %ingroup Login
     * %param[ref] _strict,_ticket
     * %permissions Any user with a valid Session.
     * %return:{LoginSession}
     *
     **%apidoc DELETE /rest/v{3-}/login/tickets/{token}
     * Invalidates a specific Ticket.
     * %caption Delete authorization Ticket
     * %ingroup Login
     * %param:string token Token that identifies the Ticket.
     * %param[ref] _strict
     * %permissions Authorization is not required.
     */
    reg("rest/v{3-}/login/tickets/:token?", GlobalPermission::none,
        std::make_unique<TicketHandler>(authenticator));
}

static void registerSystemRestHandlers(
    nx::network::rest::HandlerPool* processorPool,
    QnMediaServerModule* serverModule,
    nx::vms::network::AbstractServerConnector* serverConnector,
    CloudManagerGroup* cloudManagerGroup,
    nx::vms::utils::metrics::SystemController* systemController)
{
    const auto reg = processorPool->createRegisterHandlerFunctor();

    /**%apidoc GET /rest/v{1-3}/system/info
     * %caption Get Site info
     * %ingroup Site
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_with
     * %permissions Authorization is not required.
     * %return:{SiteInformation} Information about the Site.
     *     %param[unused] endpoint
     *     %param[unused] status
     *
     **%apidoc GET /rest/v{4-}/site/info
     * %caption Get Site info
     * %ingroup Site
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_with
     * %permissions Authorization is not required.
     * %return:{SiteInformation} Information about the Site.
     *     %param[unused] endpoint
     *     %param[unused] status
     */
    reg("rest/v{1-}/^system|site/info", GlobalPermission::none,
        std::make_unique<SiteInformationHandler>(serverModule));

    /**%apidoc GET /rest/v{1-3}/system/other
     * %caption Get info about other Sites
     * %ingroup Site
     * %struct OtherSiteRequest
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_with
     * %permissions Authorization is not required.
     * %return:{SiteInformationList} List of records about other Sites.
     *     %param servers Filled only for the info(s) from `endpoint` requested.
     *     %param devices Filled only for the info(s) from `endpoint` requested.
     *     %param edgeServerCount Amount of edge Servers in the Site. Filled only for the info(s)
     *         from `endpoint` requested.
     *     %param ldapSyncId Internal identification of LDAP connection. Filled only for the
     *         info(s) from `endpoint` requested.
     *
     **%apidoc GET /rest/v{4-}/site/other
     * %caption Get info about other Sites
     * %ingroup Site
     * %struct OtherSiteRequest
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_with
     * %permissions Authorization is not required.
     * %return:{SiteInformationList} List of records about other Sites.
     *     %param servers Filled only for the info(s) from `endpoint` requested.
     *     %param devices Filled only for the info(s) from `endpoint` requested.
     *     %param edgeServerCount Amount of edge Servers in the Site. Filled only for the info(s)
     *         from `endpoint` requested.
     *     %param ldapSyncId Internal identification of LDAP connection. Filled only for the
     *         info(s) from `endpoint` requested.
     */
    reg("rest/v{1-}/^system|site/other", GlobalPermission::none,
        std::make_unique<OtherSiteInformationHandler>(serverModule));

    /**%apidoc POST /rest/v{1-2}/system/cloudSignature
     * Generates or checks the Cloud message signature by the Cloud key.
     * %caption Generate/check Cloud signature
     * %ingroup Site
     * %struct CloudSignature
     * %param[ref] _strict,_ticket
     * %permissions Any User.
     * %return:{CloudSignature}
     */
    reg("rest/v{1-2}/system/cloudSignature", GlobalPermission::none,
        std::make_unique<SystemCloudSignatureHandler>(serverModule->globalSettings()));

    /**%apidoc POST /rest/v3/system/cloud/signature
     * Generates or checks the Cloud message signature by the Cloud key.
     * %caption Generate/check Cloud signature
     * %ingroup Site
     * %struct CloudSignature
     * %param[ref] _strict,_ticket
     * %permissions Any User.
     * %return:{CloudSignature}
     */
    reg("rest/v3/system/cloud/signature", GlobalPermission::none,
        std::make_unique<SystemCloudSignatureHandler>(serverModule->globalSettings()));

    /**%apidoc POST /rest/v{4-}/cloud/signature
     * Generates or checks the Cloud message signature by the Cloud key.
     * %caption Generate/check Cloud signature
     * %ingroup Cloud
     * %struct CloudSignature
     * %param[ref] _strict,_ticket
     * %permissions Any User.
     * %return:{CloudSignature}
     */
    reg("rest/v{4-}/cloud/signature", GlobalPermission::none,
        std::make_unique<SystemCloudSignatureHandler>(serverModule->globalSettings()));

    /**%apidoc POST /rest/v{1-3}/system/merge
     * Merges two Sites. In case of there are several servers on the merged Site
     * and they are merged partially then method returns code 202 instead of 200.
     * %caption Merge Sites
     * %ingroup Site
     * %struct SiteMergeData
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     * %return:{MergeStatusReply}
     *
     **%apidoc GET /rest/v{1-3}/system/merge
     * %caption Get Site merge status
     * %ingroup Site
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Any User.
     * %return:{MergeStatusReply}
     */
    reg("rest/v{1-3}/system/merge", GlobalPermission::none, GlobalPermission::administrator,
        std::make_unique<SiteMergeHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{4-}/site/merge
     * Merges two Sites. In case of there are several servers on the merged Site
     * and they are merged partially then method returns code 202 instead of 200.
     * %caption Merge Sites
     * %ingroup Site
     * %struct SiteMergeData
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     * %return:{MergeStatusReply}
     *
     **%apidoc GET /rest/v{4-}/site/merge
     * %caption Get Site merge status
     * %ingroup Site
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Any User.
     * %return:{MergeStatusReply}
     */
    reg("rest/v{4-}/site/merge", GlobalPermission::none, GlobalPermission::administrator,
        std::make_unique<SiteMergeHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{1-3}/system/storageEncryption
     * Adds a new key to encrypt the media data in the video archive. The key is generated from the
     * given password and salt. The password must not be empty.
     * %caption Add Storage encryption key
     * %ingroup Site
     * %struct StorageEncryptionData
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{AesKeyData} The created AES key.
     *
     **%apidoc GET /rest/v{1-3}/system/storageEncryption
     * %caption Get Storage encryption keys
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{AesKeyDataList} List of all AES keys.
     *
     **%apidoc GET /rest/v{1-3}/system/storageEncryption/{ivVect}
     * %caption Get Storage encryption key
     * %ingroup Site
     * %param:string ivVect Id of an AES key represented as the "ivVect" field of the response to
     *     `POST /rest/v{1-3}/system/storageEncryption` or
     *     `GET /rest/v{1-3}/system/storageEncryption`.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket
     * %permissions Power User.
     * %return:{AesKeyData} The requested AES key.
     *
     **%apidoc DELETE /rest/v{1-3}/system/storageEncryption/{ivVect}
     * Deletes the encryption key from the Site database.
     * %caption Delete Storage encryption key
     * %ingroup Site
     * %param:string ivVect Id of an AES key represented as the "ivVect" field of the response to
     *     `POST /rest/v{1-3}/system/storageEncryption` or
     *     `GET /rest/v{1-3}/system/storageEncryption`.
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{1-3}/system/storageEncryption/:ivVect?", GlobalPermission::powerUser,
        std::make_unique<StorageEncryptionHandler>(serverModule));

    /**%apidoc POST /rest/v{4-}/site/storageEncryption
     * Adds a new key to encrypt the media data in the video archive. The key is generated from the
     * given password and salt. The password must not be empty.
     * %caption Add Storage encryption key
     * %ingroup Site
     * %struct StorageEncryptionData
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{AesKeyData} The created AES key.
     *
     **%apidoc GET /rest/v{4-}/site/storageEncryption
     * %caption Get Storage encryption keys
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{AesKeyDataList} List of all AES keys.
     *
     **%apidoc GET /rest/v{4-}/site/storageEncryption/{ivVect}
     * %caption Get Storage encryption key
     * %ingroup Site
     * %param:string ivVect Id of an AES key represented as the "ivVect" field of the response to
     *     `POST /rest/v{4-}/site/storageEncryption` or
     *     `GET /rest/v{4-}/site/storageEncryption`.
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket
     * %permissions Power User.
     * %return:{AesKeyData} The requested AES key.
     *
     **%apidoc DELETE /rest/v{4-}/site/storageEncryption/{ivVect}
     * Deletes the encryption key from the Site database.
     * %caption Delete Storage encryption key
     * %ingroup Site
     * %param:string ivVect Id of an AES key represented as the "ivVect" field of the response to
     *     `POST /rest/v{4-}/site/storageEncryption` or
     *     `GET /rest/v{4-}/site/storageEncryption`.
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/site/storageEncryption/:ivVect?", GlobalPermission::powerUser,
        std::make_unique<StorageEncryptionHandler>(serverModule));

    /**%apidoc GET /rest/v{2-3}/system/resourceData
     * Retrieves the Device configuration file (resource_data.json) which is currently in use.
     * %caption Get Device configuration
     * %ingroup Site
     * %param[ref] _strict,_ticket
     * %permissions Any User.
     */
    reg("rest/v{2-3}/system/resourceData", GlobalPermission::none,
        std::make_unique<ResourceDataHandler>(serverModule->resourceDataPool()));

    /**%apidoc GET /rest/v{4-}/site/resourceData
     * Retrieves the Device configuration file (resource_data.json) which is currently in use.
     * %caption Get Device configuration
     * %ingroup Site
     * %param[ref] _strict,_ticket
     * %permissions Any User.
     */
    reg("rest/v{4-}/site/resourceData", GlobalPermission::none,
        std::make_unique<ResourceDataHandler>(serverModule->resourceDataPool()));

    /**%apidoc GET /rest/v{2-3}/system/metrics/manifest
     * Retrieves the manifest for `GET /rest/v{2-3}/system/metrics/alarms` and
     * `GET /rest/v{2-3}/system/metrics/values` visualization.
     * %caption Get Metrics manifest
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Metrics' Viewer.
     * %return:{std::vector<ResourceManifest>}
     */
    reg("rest/v{2-3}/system/metrics/manifest", GlobalPermission::viewMetrics,
        std::make_unique<MetricsHandler<api::metrics::SiteManifest>>(
            systemController, serverConnector));

    /**%apidoc GET /rest/v{4-}/metrics/manifest
     * Retrieves the manifest for `GET /rest/v{4-}/metrics/alarms` and
     * `GET /rest/v{4-}/metrics/values` visualization.
     * %caption Get Metrics manifest
     * %ingroup Metrics
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Metrics' Viewer.
     * %return:{std::vector<ResourceManifest>}
     */
    reg("rest/v{4-}/metrics/manifest", GlobalPermission::viewMetrics,
        std::make_unique<MetricsHandler<api::metrics::SiteManifest>>(
            systemController, serverConnector));

    /**%apidoc GET /rest/v{2-3}/system/metrics/rules
     * Retrieves the rules to calculate the final manifest and raise alarms. See metrics.md for
     * details.
     * %caption Get Metrics rules
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Metrics' Viewer.
     * %return:{std::map<QString, ResourceRules>} Structure of rules.
     */
    reg("rest/v{2-3}/system/metrics/rules", GlobalPermission::viewMetrics,
        std::make_unique<MetricsHandler<api::metrics::SiteRules>>(
            systemController, serverConnector));

    /**%apidoc GET /rest/v{4-}/metrics/rules
     * Retrieves the rules to calculate the final manifest and raise alarms. See metrics.md for
     * details.
     * %caption Get Metrics rules
     * %ingroup Metrics
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Metrics' Viewer.
     * %return:{std::map<QString, ResourceRules>} Structure of rules.
     */
    reg("rest/v{4-}/metrics/rules", GlobalPermission::viewMetrics,
        std::make_unique<MetricsHandler<api::metrics::SiteRules>>(
            systemController, serverConnector));

    /**%apidoc GET /rest/v{2-3}/system/metrics/values
     * Retrieves the current state of the Metrics values.
     * %caption Get Metrics values
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %param[opt]:boolean _formatted
     * %permissions Metrics' Viewer.
     * %return Site Metrics values structured as per the manifest.
     */
    reg("rest/v{2-3}/system/metrics/values", GlobalPermission::viewMetrics,
        std::make_unique<MetricsHandler<api::metrics::SiteValues>>(
            systemController, serverConnector));

    /**%apidoc GET /rest/v{4-}/metrics/values
     * Retrieves the current state of the Metrics values.
     * %caption Get Metrics values
     * %ingroup Metrics
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %param[opt]:boolean _formatted
     * %permissions Metrics' Viewer.
     * %return Site Metrics values structured as per the manifest.
     */
    reg("rest/v{4-}/metrics/values", GlobalPermission::viewMetrics,
        std::make_unique<MetricsHandler<api::metrics::SiteValues>>(
            systemController, serverConnector));

    /**%apidoc GET /rest/v{2-3}/system/metrics/alarms
     * Retrieves the currently active Metrics alarms.
     * %caption Get Metrics alarms
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Metrics' Viewer.
     * %return Active alarms structured as per the manifest.
     */
    reg("rest/v{2-3}/system/metrics/alarms", GlobalPermission::viewMetrics,
        std::make_unique<MetricsHandler<api::metrics::SiteAlarms>>(
            systemController, serverConnector));

    /**%apidoc GET /rest/v{4-}/metrics/alarms
     * Retrieves the currently active Metrics alarms.
     * %caption Get Metrics alarms
     * %ingroup Metrics
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Metrics' Viewer.
     * %return Active alarms structured as per the manifest.
     */
    reg("rest/v{4-}/metrics/alarms", GlobalPermission::viewMetrics,
        std::make_unique<MetricsHandler<api::metrics::SiteAlarms>>(
            systemController, serverConnector));

    /**%apidoc POST /rest/v3/system/cloud/sync
     * Initiates the immediate pulling of the data from the Cloud. Does not wait for the current
     * Cloud polling interval end. Does nothing if the Cloud pulling is already in progress. Cloud
     * polling interval can be set up using `cloudPollingIntervalS` Site setting that should be
     * greater than zero.
     * %caption Sync with Cloud services
     * %ingroup Site
     * %struct CloudSyncRequest
     * %param[unused] service
     * %param[ref] _language,_local,_strict
     * %permissions Authorization is not required.
     * %return:{std::map<CloudService, CloudPullingStatus>}
     *
     **%apidoc GET /rest/v3/system/cloud/sync
     * Retrieves the status of the Cloud pulling that is initiated each Cloud polling interval or
     * using `POST /rest/v3/system/cloud/sync` call. Cloud polling interval can be set up using
     * `cloudPollingIntervalS` Site setting that should be greater than zero.
     * %caption Cloud services Sync status
     * %ingroup Site
     * %struct CloudSyncRequest
     * %param[unused] service
     * %param[ref] _filter,_format,_stripDefault,_language,_local,_pretty,_strict,_with
     * %param[ref] _keepDefault
     * %permissions Authorization is not required.
     * %return:{std::map<CloudService, CloudPullingStatus>}
     *
     **%apidoc POST /rest/v3/system/cloud/sync/{service}
     * Initiates the immediate pulling of the data from the Cloud. Does not wait for the current
     * Cloud polling interval end. Does nothing if the Cloud pulling is already in progress. Cloud
     * polling interval can be set up using `cloudPollingIntervalS` Site setting that should be
     * greater than zero.
     * %caption Sync with Cloud service
     * %ingroup Site
     * %struct CloudSyncRequest
     * %param[ref] _language,_local,_strict
     * %permissions Authorization is not required.
     * %return:{CloudPullingStatus}
     *
     **%apidoc GET /rest/v3/system/cloud/sync/{service}
     * Retrieves the status of the Cloud pulling that is initiated each Cloud polling interval or
     * using `POST /rest/v3/system/cloud/sync` call. Cloud polling interval can be set up using
     * `cloudPollingIntervalS` Site setting that should be greater than zero.
     * %caption Cloud service Sync status
     * %ingroup Site
     * %struct CloudSyncRequest
     * %param[ref] _format,_stripDefault,_language,_local,_pretty,_strict,_with
     * %param[ref] _keepDefault
     * %permissions Authorization is not required.
     * %return:{CloudPullingStatus}
     */
    reg("rest/v3/system/cloud/sync/:service?", GlobalPermission::none,
        std::make_unique<CloudSyncHandler>(
            serverModule, cloudManagerGroup, serverConnector));

    /**%apidoc POST /rest/v{4-}/cloud/sync
     * Initiates the immediate pulling of the data from the Cloud. Does not wait for the current
     * Cloud polling interval end. Does nothing if the Cloud pulling is already in progress. Cloud
     * polling interval can be set up using `cloudPollingIntervalS` setting that should be greater
     * than zero.
     * %caption Pull data from Cloud
     * %ingroup Cloud
     * %struct CloudSyncRequest
     * %param[unused] service
     * %param[ref] _language,_local,_strict
     * %permissions Authorization is not required.
     * %return:{std::map<CloudService, CloudPullingStatus>}
     *
     **%apidoc GET /rest/v{4-}/cloud/sync
     * Retrieves the status of the Cloud pulling that is initiated each Cloud polling interval or
     * using `POST /rest/v{4-}/cloud/sync` call. Cloud polling interval can be set up using
     * `cloudPollingIntervalS` setting that should be greater than zero.
     * %caption Cloud pulling status
     * %ingroup Cloud
     * %struct CloudSyncRequest
     * %param[unused] service
     * %param[ref] _filter,_format,_stripDefault,_language,_local,_pretty,_strict,_with
     * %permissions Authorization is not required.
     * %return:{std::map<CloudService, CloudPullingStatus>}
     *
     **%apidoc POST /rest/v{4-}/cloud/sync/{service}
     * Initiates the immediate pulling of the data from the Cloud. Does not wait for the current
     * Cloud polling interval end. Does nothing if the Cloud pulling is already in progress. Cloud
     * polling interval can be set up using `cloudPollingIntervalS` setting that should be greater
     * than zero.
     * %caption Pull data from Cloud
     * %ingroup Cloud
     * %struct CloudSyncRequest
     * %param[ref] _language,_local,_strict
     * %permissions Authorization is not required.
     * %return:{CloudPullingStatus}
     *
     **%apidoc GET /rest/v{4-}/cloud/sync/{service}
     * Retrieves the status of the Cloud pulling that is initiated each Cloud polling interval or
     * using `POST /rest/v{4-}/cloud/sync` call. Cloud polling interval can be set up using
     * `cloudPollingIntervalS` setting that should be greater than zero.
     * %caption Cloud pulling status
     * %ingroup Cloud
     * %struct CloudSyncRequest
     * %param[ref] _format,_stripDefault,_language,_local,_pretty,_strict,_with
     * %permissions Authorization is not required.
     * %return:{CloudPullingStatus}
     */
    reg("rest/v{4-}/cloud/sync/:service?", GlobalPermission::none,
        std::make_unique<CloudSyncHandler>(
            serverModule, cloudManagerGroup, serverConnector));

    /**%apidoc GET /rest/v3/system/cloud/saas
     * Information about SaaS state including available and in use licensing information.
     * %caption Cloud SaaS state
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{SaasWithServices}
     */
    reg("rest/v3/system/cloud/saas", GlobalPermission::powerUser,
        std::make_unique<SiteSaasHandler>(serverModule->systemContext()->saasServiceManager()));

    /**%apidoc GET /rest/v{4-}/cloud/saas
     * Information about SaaS state including available and in use licensing information.
     * %caption Cloud SaaS state
     * %ingroup Cloud
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{SaasWithServices}
     *     %param[unused] cloudSystemId
     */
    reg("rest/v{4-}/cloud/saas", GlobalPermission::powerUser,
        std::make_unique<SiteSaasHandler>(serverModule->systemContext()->saasServiceManager()));

    /**%apidoc POST /rest/v{1-3}/system/cleanupTaxonomy
     * Cleans up the old Taxonomy data. The data remaining from any previous plugin versions will
     * be lost.
     * %caption Clean up Taxonomy data
     * %ingroup Site
     * %permissions Power User.
     * %param[ref] _strict,_ticket
     */
    reg("rest/v{1-3}/system/cleanupTaxonomy", GlobalPermission::powerUser,
        std::make_unique<CleanupTaxonomyHandler>(serverModule));

    /**%apidoc POST /rest/v{4-}/site/cleanupTaxonomy
     * Cleans up the old Taxonomy data. The data remaining from any previous plugin versions will
     * be lost.
     * %caption Clean up Taxonomy data
     * %ingroup Site
     * %permissions Power User.
     * %param[ref] _strict,_ticket
     */
    reg("rest/v{4-}/site/cleanupTaxonomy", GlobalPermission::powerUser,
        std::make_unique<CleanupTaxonomyHandler>(serverModule));

    /**%apidoc GET /rest/v3/resourceGroups
     * %caption Get Resource Groups
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{ResourceGroupList}
     */
    reg("rest/v3/resourceGroups", GlobalPermission::none,
        std::make_unique<ResourceGroupsHandler>());

    /**%apidoc GET /rest/v{4-}/resourceGroups
     * %caption Get Resource Groups
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{ResourceGroupList}
     */
    reg("rest/v{4-}/resourceGroups", GlobalPermission::none,
        std::make_unique<ResourceGroupsHandler>());

    /**%apidoc GET /rest/v{4-}/site/resources/{id}
     * Retrieves the specified Resource info in the Site.
     * %caption Get Resource info
     * %ingroup Site
     * %param:uuid id Resource id.
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{ResourceApiInfo} Get information about the Resource.
     *
     **%apidoc GET /rest/v{4-}/site/resources
     * Retrieves all Resources' info accessible to the User in the Site.
     * %caption Get all Resources' info
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{ResourceApiInfoList} Get information about all Resources in the Site.
     */
    reg("rest/v{4-}/site/resources/:id?", GlobalPermission::none,
        std::make_unique<ResourcesHandler>(serverModule->systemContext()));
}

static void registerServerRestHandlers(
    nx::network::rest::HandlerPool* processorPool,
    QnMediaServerModule* serverModule,
    nx::vms::network::AbstractServerConnector* serverConnector,
    CloudManagerGroup* cloudManagerGroup,
    MediaServerProcess* process)
{
    const auto reg = processorPool->createRegisterHandlerFunctor();

    /**%apidoc GET /rest/v3/servers/{id}/info
     * %caption Get Server info
     * %ingroup Servers
     * %struct ServerInfoFilter
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_with
     * %param[ref] _local,_strict
     * %permissions Authorization is not required.
     * %return:{ServerInformationV1} Get information about the Server.
     *     %param[unused] ecDbReadOnly
     *
     **%apidoc GET /rest/v3/servers/%2A/info
     * %caption Get all Servers' info
     * %ingroup Servers
     * %struct ServerInfoFilter
     * %param[unused] id
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_with
     * %param[ref] _local,_strict
     * %permissions Authorization is not required.
     * %return:{ServerInformationV1List} Get information about all Servers in the Site.
     *     %param[unused] ecDbReadOnly
     *
     **%apidoc GET /rest/v{1-2}/servers/{id}/info
     * %caption Get Server info
     * %ingroup Servers
     * %struct ServerInfoFilter
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_with
     * %param[ref] _local,_strict
     * %permissions Authorization is not required.
     * %return:{ServerInformationV1} Get information about the Server.
     *
     **%apidoc GET /rest/v{1-2}/servers/%2A/info
     * %caption Get all Servers' info
     * %ingroup Servers
     * %struct ServerInfoFilter
     * %param[unused] id
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_with
     * %param[ref] _local,_strict
     * %permissions Authorization is not required.
     * %return:{ServerInformationV1List} Get information about all Servers in the Site.
     */
    reg("rest/v{1-3}/servers/:id/info", GlobalPermission::none,
        std::make_unique<ServerInformationHandler<nx::vms::api::ServerInformationV1>>(serverConnector));

    /**%apidoc GET /rest/v{4-}/servers/{id}/info
     * %caption Get Server info
     * %ingroup Servers
     * %struct ServerInfoFilter
     * %param[ref] _format,_stripDefault,_language,_pretty,_with
     * %param[ref] _local,_strict
     * %permissions Authorization is not required.
     * %return:{ServerInformationV4} Get information about the Server.
     *
     **%apidoc GET /rest/v{4-}/servers/%2A/info
     * %caption Get all Servers' info
     * %ingroup Servers
     * %struct ServerInfoFilter
     * %param[unused] id
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_with
     * %param[ref] _local,_strict
     * %permissions Authorization is not required.
     * %return:{ServerInformationV4List} Get information about all Servers in the Site.
     */
    reg("rest/v{4-}/servers/:id/info", GlobalPermission::none,
        std::make_unique<ServerInformationHandler<nx::vms::api::ServerInformationV4>>(serverConnector));

    /**%apidoc GET /rest/v{2-}/servers/{id}/runtimeInfo
     * %caption Get Server runtime info
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Any User.
     * %return:{ServerRuntimeInformation} Get the runtime information about the Server.
     *
     **%apidoc GET /rest/v{2-}/servers/%2A/runtimeInfo
     * %caption Get all Servers' runtime info
     * %ingroup Servers
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Any User.
     * %return:{ServerRuntimeInformationList} Get runtime information about all Servers
     *     in the Site.
     *
     **%apidoc PATCH /rest/v{2-}/servers/{id}/runtimeInfo
     * %caption Modify Server port
     * %ingroup Servers
     * %struct[opt] ServerPortInformation
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Administrator with a fresh session.
     * %return:{ServerRuntimeInformation} Runtime information about the Server.
     */
    reg("rest/v{2-}/servers/:id/runtimeInfo", GlobalPermission::none, GlobalPermission::administratorWithFreshSession,
        std::make_unique<ServerRuntimeInformationHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{2-}/servers/{id}/logSettings
     * %caption Get Server log settings
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{ServerLogSettings} Server log settings.
     *
     * %apidoc GET /rest/v{2-}/servers/%2A/logSettings
     * %caption Get all Servers' log settings
     * %ingroup Servers
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{std::map<nx::Uuid, ServerLogSettings>} Log settings for every Server in the Site.
     *
     * %apidoc PUT /rest/v{2-}/servers/{id}/logSettings
     * %caption Set Server log settings
     * %ingroup Servers
     * %struct ServerLogSettings
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _local,_strict,_ticket
     * %param[unused] directory
     * %permissions Power User with a fresh session.
     * %return:{ServerLogSettings} Server log settings.
     *
     * %apidoc PUT /rest/v{2-}/servers/%2A/logSettings
     * %caption Set all Servers' log settings
     * %ingroup Servers
     * %struct ServerLogSettings
     * %param[unused] id
     * %param[unused] directory
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{std::map<nx::Uuid, ServerLogSettings>} Log Settings for every Server in the Site.
     *
     * %apidoc PATCH /rest/v{2-}/servers/{id}/logSettings
     * %caption Modify Server log settings
     * %ingroup Servers
     * %struct[opt] ServerLogSettings
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[unused] directory
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{ServerLogSettings} Server log settings.
     *
     * %apidoc PATCH /rest/v{2-}/servers/%2A/logSettings
     * %caption Modify Servers' log settings
     * %ingroup Servers
     * %struct[opt] ServerLogSettings
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{std::map<nx::Uuid, ServerLogSettings>} Log settings for every Server in the Site.
     */
    reg("rest/v{2-}/servers/:id/logSettings", GlobalPermission::powerUser, GlobalPermission::powerUserWithFreshSession,
        std::make_unique<LogSettingsHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{2-}/servers/{id}/logArchive
     * %caption Get log archive
     * %ingroup Servers
     * %struct LogArchiveFilter
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _language,_local,_strict,_ticket
     * %permissions Power User.
     * %return Archive with the log files.
     */
    reg("rest/v{2-}/servers/:id/logArchive", GlobalPermission::powerUser,
        std::make_unique<LogArchiveHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{2-}/servers/{id}/staticWebContent
     * %caption Get static web content info
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %ingroup Servers
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Any User.
     * %return:{StaticWebContentInfo}
     *     %param[unused] id
     *
     **%apidoc PUT /rest/v{2-}/servers/{id}/staticWebContent
     * Starts a download from a specified URL, the download status and progress can be
     * monitored with `GET /rest/v{2-}/servers/{id}/staticWebContent` request. A successfully
     * downloaded archive replaces the static web content. Another `PUT` request cancels
     * the current download and starts a new one.
     * %caption Download static web content
     * %ingroup Servers
     * %struct StaticWebContentInfo
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _local,_strict,_ticket
     * %permissions Administrator with a fresh session.
     * %return:{StaticWebContentInfo}
     *     %param[unused] id
     *
     **%apidoc PUT /rest/v{2-}/servers/{id}/staticWebContent/upload
     * Uploads the static web content directly as a binary archive. This request cancels any
     * background download and forcefully sets the uploaded content as the static web content.
     * %caption Upload static web content
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _local,_strict,_ticket
     * %permissions Administrator with a fresh session.
     * %return:{StaticWebContentInfo}
     *     %param[unused] id
     *
     **%apidoc DELETE /rest/v{2-}/servers/{id}/staticWebContent
     * Resets the static web content to the one that comes with the installation. This request
     * cancels any background download.
     * %caption Reset static web content
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _local
     * %param[ref] _strict,_ticket
     * %ingroup Servers
     * %permissions Administrator with a fresh session.
     */
    reg("rest/v{2-}/servers/:id/staticWebContent/:_upload?", GlobalPermission::none, GlobalPermission::administratorWithFreshSession,
        std::make_unique<StaticWebContentHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{1-}/servers/{id}/backupSettings
     * %caption Get Server Backup settings
     * %ingroup Server Data
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{BackupSettings} Server Backup settings.
     *
     * %apidoc GET /rest/v{1-}/servers/%2A/backupSettings
     * %caption Get Servers' Backup settings
     * %ingroup Server Data
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{BackupSettingsList} Backup settings of all Servers.
     *
     * %apidoc POST /rest/v{1-}/servers/{id}/backupSettings
     * %caption Set Server Backup settings
     * %ingroup Server Data
     * %struct BackupSettings
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{BackupSettings} Backup settings.
     */
    reg("rest/v{1-}/servers/:id/backupSettings", GlobalPermission::powerUser,
        std::make_unique<BackupSettingsHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{1-3}/servers/{serverId}/backupPositions
     * Retrieves Backup Positions for all Devices from the Server specified by `serverId`.
     * %caption Get Server Backup Positions
     * %ingroup Server Data
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{BackupPositionExV1List} List of actual Backup Positions for all Devices on the
     *     Server.
     *
     **%apidoc GET /rest/v{1-3}/servers/{serverId}/backupPositions/{deviceId}
     * Retrieves the Backup Position for the Device specified by `deviceId` from the Server
     * specified by `serverId`. In the case the backup cannot be continued because of a critical
     * storage issues, "positionLowMs", "positionHighMs" and "bookmarkStartPositionMs" will equal
     * `-2`.
     * %caption Get Device Backup Position
     * %ingroup Server Data
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{BackupPositionExV1} Backup Position for the given Device.
     *
     **%apidoc PUT /rest/v{1-3}/servers/{serverId}/backupPositions/{deviceId}
     * Sets the Backup Position for the Device specified by `deviceId` and the Server specified by
     * `serverId`. Note that "positionLowMs" and "positionHighMs" cannot be moved to the past, only
     * to the future.
     * %caption Set Device Backup Position
     * %ingroup Server Data
     * %struct BackupPositionV1
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{BackupPositionV1} Backup Position.
     *
     **%apidoc PUT /rest/v{1-3}/servers/{serverId}/backupPositions
     * Sets the Backup Position for all Devices for the Server specified by `serverId`. Note that
     * "positionLowMs" and "positionHighMs" can not be moved to the past, only to the future.
     * %caption Set Devices' Backup Positions
     * %ingroup Server Data
     * %struct BackupPositionV1
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param[unused] deviceId
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{BackupPositionV1List} Backup Positions.
     *
     **%apidoc POST /rest/v{2-3}/servers/{serverId}/backupPositions/{deviceId}/reset
     * Resets the Backup Position for the Device specified by `deviceId` on the Server specified
     * by `serverId`. Note that `deviceId` might be equal to "*", In this case Backup Position
     * is reset for all devices on this server.
     * Backup process will start from scratch. Note that previously
     * backed up data will not be deleted. Only data present in the main archive but missing
     * in the backup will be processed.
     * %caption Reset Device Backup Position
     * %ingroup Server Data
     * %struct ServerAndDeviceIdData
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{1-3}/servers/:serverId/backupPositions/:deviceId?/:_reset?", GlobalPermission::powerUser,
        std::make_unique<BackupPositionHandler<nx::vms::api::BackupPositionExV1>>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/servers/{serverId}/backupPositions
     * Retrieves Backup Positions for all Devices from the Server specified by `serverId`.
     * %caption Get Server Backup Positions
     * %ingroup Server Data
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{BackupPositionExV4List} List of actual Backup Positions for all Devices on the
     *     Server.
     *
     **%apidoc GET /rest/v{4-}/servers/{serverId}/backupPositions/{deviceId}
     * Retrieves the Backup Position for the Device specified by `deviceId` from the Server
     * specified by `serverId`. In the case the backup cannot be continued because of a critical
     * storage issues, "positionLowMs", "positionHighMs" and "bookmarkStartPositionMs" will equal
     * `-2`.
     * %caption Get Device Backup Position
     * %ingroup Server Data
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{BackupPositionExV4} Backup Position for the given Device.
     *
     **%apidoc PUT /rest/v{4-}/servers/{serverId}/backupPositions/{deviceId}
     * Sets the Backup Position for the Device specified by `deviceId` and the Server specified by
     * `serverId`. Note that "positionLowMs" and "positionHighMs" cannot be moved to the past, only
     * to the future.
     * %caption Set Device Backup Position
     * %ingroup Server Data
     * %struct BackupPositionV4
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{BackupPositionV4} Backup Position.
     *
     **%apidoc PUT /rest/v{4-}/servers/{serverId}/backupPositions
     * Sets the Backup Position for all Devices for the Server specified by `serverId`. Note that
     * "positionLowMs" and "positionHighMs" can not be moved to the past, only to the future.
     * %caption Set Devices' Backup Positions
     * %ingroup Server Data
     * %struct BackupPositionV4
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param[unused] deviceId
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{BackupPositionV4List} Backup Positions.
     *
     **%apidoc POST /rest/v{4-}/servers/{serverId}/backupPositions/{deviceId}/reset
     * Resets the Backup Position for the Device specified by `deviceId` on the Server specified
     * by `serverId`. Note that `deviceId` might be equal to "*", In this case Backup Position
     * is reset for all devices on this server.
     * Backup process will start from scratch. Note that previously
     * backed up data will not be deleted. Only data present in the main archive but missing
     * in the backup will be processed.
     * %caption Reset Device Backup Position
     * %ingroup Server Data
     * %struct ServerAndDeviceIdData
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/servers/:serverId/backupPositions/:deviceId?/:_reset?", GlobalPermission::powerUser,
        std::make_unique<BackupPositionHandler<nx::vms::api::BackupPositionExV4>>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/servers/{serverId}/qrcode
     * Get QR Code from the Server specified by {serverId}.
     * %caption Get Server QR code
     * %ingroup Server Data
     * %struct QrCodeRequestData
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{QrCodeData} QR Code data
     *
     **%apidoc GET /rest/v{4-}/servers/%2A/qrcode
     * Get QR Codes from all Servers.
     * %caption Get all Servers QR codes
     * %ingroup Server Data
     * %struct QrCodeRequestData
     * %param[unused] serverId
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{QrCodeDataList} QR code data per Server.
     */
    reg("rest/v{4-}/servers/:serverId/qrcode", GlobalPermission::powerUser,
        std::make_unique<QrCodeHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/servers/{serverId}/qrcode/image
     * Get QR Code image from the Server specified by {serverId}.
     * %caption Get Server QR code image
     * %ingroup Server Data
     * %struct QrCodeRequestData
     * %param[ref] _strict,_ticket
     * %param[ref] _local
     * %permissions Power User.
     */
    reg("rest/v{4-}/servers/:serverId/qrcode/image", GlobalPermission::powerUser,
        std::make_unique<QrCodeImageHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{2-}/servers/{serverId}/remoteArchive/{deviceId}/sync
     * Retrieves the Remote Archive synchronization status for the Device specified by {deviceId}
     * from the Server specified by {serverId}.
     * %caption Get Remote Archive status
     * %ingroup Server Data
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{RemoteArchiveSynchronizationStatus}
     *     Remote Archive Synchronization status.
     *
     **%apidoc GET /rest/v{2-}/servers/{serverId}/remoteArchive/%2A/sync
     * Retrieves the Remote Archive synchronization status list for Devices from the Server
     * specified by {serverId}.
     * %caption Get Remote Archive statuses
     * %ingroup Server Data
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{RemoteArchiveSynchronizationStatusList}
     *      List of Remote Archive Synchronization Statuses
     */
    reg("rest/v{2-}/servers/:serverId/remoteArchive/:deviceId/sync",
        GlobalPermission::powerUser,
        std::make_unique<RemoteArchiveSynchronizationStatusHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{1-}/servers/{id}/dbBackups
     * Creates a Site database Backup on the Server specified by {id}.
     * %caption Back up Site DB
     * %ingroup Server Data
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[opt]:string reason Reason of the backup creation. Default is "manual".
     *     %example manual
     * %param[ref] _local,_strict,_ticket
     * %permissions Administrator.
     * %return:{StoredFilePath} Location on the Server of the created DB backup file.
     *
     **%apidoc GET /rest/v{1-}/servers/{id}/dbBackups
     * %caption List Site DB Backups
     * %ingroup Server Data
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _filter,_format,_language,_pretty,_strict,_ticket
     * %param[ref] _local
     * %permissions Administrator.
     * %return:{StoredFilePathList} List of the DB backup file locations on the Server.
     */
    reg("rest/v{1-}/servers/:id/dbBackups", GlobalPermission::administrator,
        std::make_unique<ServerBackupDbHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{1-}/servers/{id}/detach
     * Detaches the Server specified by {id} from the Site. This means that:
     * - The Server local Site id will be reset.
     * - The Server admin password will be set to default.
     * - All Cloud Users will be deleted from the Server DB (if any).
     * - The Server will be unbound from the Cloud (if it was bound).
     * - The Server will be disconnected from all the other Servers in the Site.
     * %caption Detach Server
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from the "id" field via
     *     `GET /rest/v{1-}/servers`, or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _local,_strict,_ticket
     * %permissions Administrator with a fresh session.
     */
    reg("rest/v{1-}/servers/:id/detach", GlobalPermission::administratorWithFreshSession,
        std::make_unique<ServerDetachHandler>(
            serverModule, serverConnector, cloudManagerGroup));

    /**%apidoc POST /rest/v{1-}/servers/{id}/reset
     * Resets the Server specified by {id} to the initial state, i.e. <b>deletes the Server
     * database</b>. The Server will restart after executing this command.
     * %caption Reset Server
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _local,_strict,_ticket
     * %permissions Administrator with a fresh session.
     */
    reg("rest/v{1-}/servers/:id/reset", GlobalPermission::administratorWithFreshSession,
        std::make_unique<ServerResetHandler>(serverModule, serverConnector, process));

    /**%apidoc POST /rest/v{1-}/servers/{id}/restart
     * Restarts the Server specified by {id}.
     * %caption Restart Server
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User with a fresh session.
     */
    reg("rest/v{1-}/servers/:id/restart", GlobalPermission::powerUserWithFreshSession,
        std::make_unique<ServerRestartHandler>(serverModule, serverConnector, process));

    /**%apidoc[proprietary] GET /rest/v{2-}/servers/{id}/p2pStats
     * %caption Get Server P2P stats
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{P2pStats} P2P Stats.
     *
     **%apidoc[proprietary] GET /rest/v{2-}/servers/%2A/p2pStats
     * Retrieves P2P stats for all Servers
     * %caption Get P2P stats of all Servers
     * %ingroup Servers
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{P2pStatsList} List of P2P stats.
     */
    reg("rest/v{2-}/servers/:id/p2pStats", GlobalPermission::powerUser,
        std::make_unique<P2pStatsHandler>(serverConnector));

    /**%apidoc[proprietary] GET /rest/v{2-}/servers/{id}/iniConfig
     * Intended for debugging and experimenting. Retrieves the current state of the IniConfig
     * mechanism, including the directory used for .ini files. Note that this directory is also
     * used for other mechanisms like Output Redirector and log configuration snippets.
     * %caption Get IniConfig info
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket
     * %param[ref] _local
     * %permissions Power User.
     * %return:{IniConfigModel}
     *
     **%apidoc[proprietary] GET /rest/v{2-}/servers/%2A/iniConfig
     * %caption Get Servers' IniConfig info
     * %ingroup Servers
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket
     * %param[ref] _local
     * %permissions Power User.
     * %return:{IniConfigModelList}
     */
    reg("rest/v{2-}/servers/:id/iniConfig", GlobalPermission::powerUser,
        std::make_unique<IniConfigHandler>(serverConnector));

    /**%apidoc GET /rest/v{2-}/servers/{id}/recordingStatistics
     * Retrieves the Server recording statistics.
     * %caption Get Server recording stats
     * %ingroup Server Data
     * %struct RecordingStatisticsFilter
     * %param[ref] _local
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{RecordingStatisticsDataList} Recording statistics for the Server.
     *
     **%apidoc GET /rest/v{2-}/servers/%2A/recordingStatistics
     * Retrieves the recording statistics for all Servers.
     * %caption Get Servers' recording stats
     * %ingroup Server Data
     * %struct RecordingStatisticsFilter
     * %param[ref] _local
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{std::map<nx::Uuid, RecordingStatisticsDataList>} Recording statistics for all Servers.
     */
    reg("rest/v{2-}/servers/:id/recordingStatistics", GlobalPermission::powerUser,
        std::make_unique<RecordingStatisticsHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{2-3}/servers/{id}/settingsManifest
     * Retrieves the settings manifest of the Server.
     * %caption Get Server settings manifest
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{SettingsDescription} Settings manifest of the Server.
     *
     **%apidoc GET /rest/v{2-3}/servers/%2A/settingsManifest
     * Retrieves the settings manifests of all Servers.
     * %caption Get Server settings manifests
     * %ingroup Servers
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{std::map<nx::Uuid, SettingsDescription>} Setting manifests of Servers.
     */
    reg("rest/v{2-3}/servers/:id/settingsManifest", GlobalPermission::powerUser,
        std::make_unique<ServerSettingsManifestHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/servers/{id}/settings
     * %caption Server settings information
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`
     *     or be `this` to refer to the current Server, or be "*" to involve all Servers.
     *     %example this
     * %param[ref] _format,_stripDefault,_language,_local,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{SettingsInfo}
     *
     **%apidoc GET /rest/v{4-}/servers/%2A/settings
     * %caption Servers config information
     * %ingroup Servers
     * %param[ref] _filter,_format,_stripDefault,_language,_local,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{SettingsInfoList}
     */
    reg("rest/v{4-}/servers/:id/settings", GlobalPermission::none,
        std::make_unique<SettingsHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{2-}/servers/{id}/storageForecast
     * Retrieves the Server Storage forecast.
     * %caption Get Server Storage forecast
     * %ingroup Server Data
     * %struct StorageForecastData
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{StorageForecastList} Storage forecast for the Server.
     *
     **%apidoc GET /rest/v{2-}/servers/%2A/storageForecast
     * Retrieves all Storage forecasts for all Servers.
     * %caption Get Servers' Storage forecasts
     * %ingroup Server Data
     * %struct StorageForecastData
     * %param[ref] _local
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{std::map<nx::Uuid, StorageForecastList>} Storage forecast for all Servers.
     */
    reg("rest/v{2-}/servers/:id/storageForecast", GlobalPermission::powerUser,
        std::make_unique<StorageForecastHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{1-}/servers/{id}/storages/%2A/purge
     * Starts Storage purge for the Server specified by {id}.
     * %caption Start Storage purge
     * %ingroup Server Data
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param:enum pool
     *     %value main
     *     %value backup
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User.
     * %return:{StoragePurgeStatusData} Storage purge status.
     *
     **%apidoc GET /rest/v{1-}/servers/{id}/storages/%2A/purge
     * Retrieves the Storage purge status for the Server specified by {id}.
     * %caption Get Storage purge status
     * %ingroup Server Data
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param:enum pool
     *     %value main
     *     %value backup
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{StoragePurgeStatusData} Storage purge status.
     *
     **%apidoc GET /rest/v{1-}/servers/%2A/storages/%2A/purge
     * Retrieves the Storage purge statuses for all Servers.
     * %caption Get all Storage purge statuses
     * %ingroup Server Data
     * %param:enum pool
     *     %value main
     *     %value backup
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{StoragePurgeStatusDataList} Storage purge status list.
     */
    reg("rest/v{1-}/servers/:id/storages/*/purge", GlobalPermission::powerUser,
        std::make_unique<StoragePurgeHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{1-2}/servers/{serverId}/storages/{id}/status
     * Retrieves the existing Storage status information object specified by {id} for the Server
     * specified by {serverId}.
     * %caption Get Storage status
     * %ingroup Server Data
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param:uuid id Storage id (can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers/{serverId}/storages`).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{StorageSpaceDataWithDbInfoV1} Storage status information object.
     *
     **%apidoc GET /rest/v{1-2}/servers/{serverId}/storages/%2A/status
     * Retrieves the existing Storage status information objects for the Server specified by
     * {serverId}.
     * %caption Get all Storages' statuses
     * %ingroup Server Data
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server, or be "*" to
     *     involve all Servers.
     *     %example this
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{StorageSpaceDataWithDbInfoV1List} Storage status information objects.
     */
    reg("rest/v{1-2}/servers/:serverId/storages/:id/status", GlobalPermission::powerUser,
        std::make_unique<StorageStatusHandler<nx::vms::api::StorageSpaceDataWithDbInfoV1>>(
            serverModule, serverConnector));

    /**%apidoc GET /rest/v{3-}/servers/{serverId}/storages/{id}/status
     * Retrieves the existing Storage status information object specified by {id} for the Server
     * specified by {serverId}.
     * %caption Get Storage status
     * %ingroup Server Data
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param:uuid id Storage id (can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers/{serverId}/storages`).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{StorageSpaceDataWithDbInfoV3} Storage status information object.
     *
     **%apidoc GET /rest/v{3-}/servers/{serverId}/storages/%2A/status
     * Retrieves the existing Storage status information objects for the Server specified by
     * {serverId}.
     * %caption Get all Storages' statuses
     * %ingroup Server Data
     * %param:string serverId Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server, or be "*" to
     *     involve all Servers.
     *     %example this
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{StorageSpaceDataWithDbInfoV3List} Storage status information objects.
     */
    reg("rest/v{3-}/servers/:serverId/storages/:id/status", GlobalPermission::powerUser,
        std::make_unique<StorageStatusHandler<nx::vms::api::StorageSpaceDataWithDbInfoV3>>(
            serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/servers/{serverId}/storages/%2A/check
     * Check whether the specified location could be used as a Storage path for the Server
     * specified by {serverId} and report the location details.
     * If the location is already used as the Server Storage then `storageId` is expected in response.
     * %caption Check Storage path
     * %ingroup Server Data
     * %struct StorageCheckFilter
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{StorageSpaceDataWithDbInfoV3} Storage status information object.
     */
    reg("rest/v{4-}/servers/:serverId/storages/*/check", GlobalPermission::powerUser,
        std::make_unique<StorageCheckHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{2-}/servers/{id}/rebuildArchive
     * Retrieves the status of rebuilding of the Server "main" and "backup" Archives.
     * %caption Get Archives rebuild status
     * %ingroup Server Data
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers` or
     *     be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{std::map<QString, StorageScanInfo>} Archive rebuilding process information
     *     object per Archive location.
     *
     **%apidoc GET /rest/v{2-}/servers/%2A/rebuildArchive
     * Retrieves the status of the "main" and "backup" Archive rebuilding from all Servers.
     * %caption Get Archives rebuild statuses
     * %ingroup Server Data
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{std::map<nx::Uuid, std::map<QString, StorageScanInfo>>} Storage rebuild process information object.
     *
     **%apidoc GET /rest/v{2-}/servers/{id}/rebuildArchive/{location}
     * Retrieves the status of the Server "main" or "backup" Archive rebuilding.
     * %caption Get Archive rebuild status
     * %ingroup Server Data
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers` or
     *     be `this` to refer to the current Server.
     *     %example this
     * %param:enum location
     *     %value main
     *     %value backup
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{std::map<QString, StorageScanInfo>} Archive rebuilding process information object
     *     per Archive location.
     *
     **%apidoc GET /rest/v{2-}/servers/%2A/rebuildArchive/{location}
     * Retrieves the status of the "main" or "backup" Archive rebuilding from all Servers.
     * %caption Get Archives rebuild status
     * %ingroup Server Data
     * %param:enum location
     *     %value main
     *     %value backup
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{std::map<nx::Uuid, std::map<QString, StorageScanInfo>>} Storage rebuild process information object.
     *
     **%apidoc POST /rest/v{2-}/servers/{id}/rebuildArchive
     * Starts the Server "main" and "backup" Archive rebuilding.
     * %caption Start rebuilding Archives
     * %ingroup Server Data
     * %struct[opt] RebuildArchiveFilter
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User.
     * %return:{std::map<QString, StorageScanInfo>} Archive rebuilding process information object
     *     per Archive location.
     *
     **%apidoc POST /rest/v{2-}/servers/{id}/rebuildArchive/{location}
     * Starts rebuilding the Server "main" or "backup" Archive.
     * %caption Start rebuilding Archive
     * %ingroup Server Data
     * %struct[opt] RebuildArchiveFilter
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User.
     * %return:{std::map<QString, StorageScanInfo>} Archive rebuilding process information object
     *     per Archive location.
     *
     **%apidoc DELETE /rest/v{2-}/servers/{id}/rebuildArchive
     * Stops rebuilding the Server Archive.
     * %caption Stop rebuilding Archives
     * %ingroup Server Data
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _local
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     *
     **%apidoc DELETE /rest/v{2-}/servers/%2A/rebuildArchive
     * Stops rebuilding the Archives for all Servers.
     * %caption Stop rebuilding all Archives
     * %ingroup Server Data
     * %param[ref] _local
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     *
     **%apidoc DELETE /rest/v{2-}/servers/{id}/rebuildArchive/{location}
     * Stops rebuilding the Server "main" or "backup" Archive.
     * %caption Stop rebuilding Archive
     * %ingroup Server Data
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers`,
     *     or be `this` to refer to the current Server.
     *     %example this
     * %param:enum location
     *     %value main
     *     %value backup
     * %param[ref] _local
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     *
     **%apidoc DELETE /rest/v{2-}/servers/%2A/rebuildArchive/{location}
     * Stops rebuilding the "main" or "backup" Archives for all Servers.
     * %caption Stop rebuilding Archives
     * %ingroup Server Data
     * %param:enum location
     *     %value main
     *     %value backup
     * %param[ref] _local
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{2-}/servers/:id/rebuildArchive/:location?", GlobalPermission::powerUser,
        std::make_unique<RebuildArchiveHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{3-}/servers/{id}/timeZones
     * Retrieves the complete list of time zones supported by the Server machine.
     * %caption Get Time Zones
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket
     * %param[ref] _local
     * %permissions Any User.
     * %return:{TimeZonesDataList} List of objects representing the time zones.
     */
    reg("rest/v{3-}/servers/:id/timeZones", GlobalPermission::none,
        std::make_unique<TimeZonesHandler>(serverModule->peerId(), serverConnector));

    /**%apidoc GET /rest/v{3-}/servers/{id}/scripts
     * Retrieves the complete list of scripts to available to execute by the Server machine.
     * %caption Get scripts
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via
     *     `GET /rest/v{1-}/servers` or be `this` to refer to the current Server, or be "*" to
     *     involve all Servers.
     *     %example this
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power user.
     * %return:{std::vector<std::string>} List of scripts available on server.
     */
    reg("rest/v{3-}/servers/:id/scripts", GlobalPermission::powerUser,
        std::make_unique<ScriptsHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{3-}/servers/{id}/scripts/{name}/run
     * Execute a script on the Server. Use `/rest/v{3-}/servers/{id}/scripts` to get the available
     * scripts.
     * %caption Execute script
     * %ingroup Servers
     * %struct ScriptInfo
     * %param[ref] _local,_strict,_ticket
     * %permissions Power user.
     * %return:{RunProcessResult} Result of executed process
     */
    reg("rest/v{3-}/servers/:id/scripts/:name/run", GlobalPermission::powerUser,
        std::make_unique<RunScriptHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{3-}/servers/{serverId}/testEmail
     * Performs test connection to the SMTP service described in the parameter by the built-in
     * SMTP client. If parameter is omitted, the same set of the data fields will be retrieved from
     * the Site Settings. Thus, this method may be used both for checking potential and currently
     * in use SMTP services.
     * %caption Test Email Settings
     * %ingroup Servers
     * %struct EmailSettingsRequest
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User.
     * %return:{EmailSettingsReply} Result of settings request.
     */
    reg("rest/v{3-}/servers/:serverId/testEmail", GlobalPermission::powerUser,
        std::make_unique<TestEmailHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{3-}/servers/{serverId}/pingDevices
     * Queries the given list of device UUIDs over the network and returns the list of of those
     * UUIDs that are available
     * %caption Ping Devices
     * %ingroup Servers
     * %struct ConnectedDevicesRequest
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User.
     * %return:{DeviceModelGeneralList} List of connected devices.
     */
    reg("rest/v{3-}/servers/:serverId/pingDevices", GlobalPermission::powerUser,
        std::make_unique<PingDevicesHandler>(serverModule, serverConnector));

    /**%apidoc[proprietary] GET /rest/v{3-}/servers/{id}/nvrNetwork
     * Retrieves the NVR network state. Works only on a VMS Server installed on an NVR device.
     * %caption NVR Network
     * %ingroup Servers
     * %param:string id Server id. Can be obtained from "id" field via `GET /rest/v{1-}/servers` or
     *     be `this` to refer to the current Server.
     *     %example this
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket
     * %param[ref] _with,_local
     * %permissions Power User.
     * %return:{NetworkBlockData} Network Block Result.
     *
     * %apidoc[proprietary] PUT /rest/v{3-}/servers/{id}/nvrNetwork
     * Configures the NVR network state. Works only on a VMS Server installed on an NVR device.
     * %caption NVR Network
     * %ingroup Servers
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User.
     * %struct NvrNetworkRequest
     * %return:{NetworkBlockData} Network Block Result.
     */
    reg("rest/v{3-}/servers/:id/nvrNetwork", GlobalPermission::powerUser,
        std::make_unique<NvrNetworkHandler>(serverModule, serverConnector));

    /**%apidoc[proprietary] GET /rest/v{3-}/servers/{id}/overlappedIds/{groupId}
     * Retrieves the Overlapped Id in use on the NVR. Works only on a VMS Server connected to an
     * NVR which supportes the Overlapped Id feature.
     * %caption Overlapped Ids
     * %ingroup Servers
     * %struct OverlappedIdsRequest
     * %param[unused] overlappedId
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket
     * %param[ref] _with,_local
     * %permissions Any User.
     * %return:{OverlappedIdResponse} Overlapped Id Settings.
     *
     * %apidoc[proprietary] PUT /rest/v{3-}/servers/{id}/overlappedIds/{groupId}
     * Sets the Overlapped Id to use on the NVR. Works only on a VMS Server connected to an
     * NVR which supportes the Overlapped Id feature.
     * %caption Overlapped Ids
     * %ingroup Servers
     * %struct OverlappedIdsRequest
     * %param[ref] _local,_strict,_ticket
     * %permissions Any User.
     * %return:{OverlappedIdResponse} Overlapped Id Settings.
     */
    reg("rest/v{3-}/servers/:id/overlappedIds/:groupId", GlobalPermission::none,
        std::make_unique<OverlappedIdsHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/servers/{serverId}/audit
     * %caption Get Audit log
     * %ingroup Servers
     * %struct AuditLogFilter
     * %param[ref] _format,_stripDefault,_language,_local,_pretty,_strict,_ticket,_with
     * %permissions Administrator.
     * %return:{AuditRecordList}
     */
    reg("rest/v{4-}/servers/:serverId/audit", GlobalPermission::administrator,
        std::make_unique<AuditHandler>(serverModule, serverConnector));
}

static void registerDeviceHandlers(
    nx::network::rest::HandlerPool* processorPool,
    QnMediaServerModule* serverModule,
    nx::vms::network::AbstractServerConnector* serverConnector,
    auth::Authenticator* authenticator)
{
    const auto reg = processorPool->createRegisterHandlerFunctor();

    /**%apidoc GET /rest/v{1-2}/devices/%2A/bookmarks
     * Retrieves the Bookmark records stored in the Site.
     * %caption Get Bookmarks
     * %ingroup Device Media
     * %struct BookmarkFilterV1
     *     %param[unused] id
     *     %param[opt] deviceId
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions View bookmarks.
     * %return:{BookmarkV1List} List of Bookmark records.
     *
     **%apidoc GET /rest/v{1-2}/devices/{deviceId}/bookmarks
     * Retrieves the Bookmark records stored in the Site for the particular Device.
     * %caption Get Device Bookmarks
     * %ingroup Device Media
     * %struct BookmarkFilterV1
     *     %param[unused] id
     *     %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *         field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions View bookmarks.
     * %return:{BookmarkV1List} List of Bookmark records.
     *
     **%apidoc POST /rest/v{1-2}/devices/{deviceId}/bookmarks
     * Creates a record in the Site for the new Bookmark.
     * %caption Create Bookmark
     * %ingroup Device Media
     * %struct BookmarkV1
     *     %param[unused] creationTimeMs
     *     %param[unused] id
     * %param[ref] _local,_strict,_ticket
     * %permissions Manage bookmarks.
     * %return:{BookmarkV1} Bookmark record that was created.
     *
     **%apidoc GET /rest/v{1-2}/devices/{deviceId}/bookmarks/{id}
     * Retrieves the particular Bookmark record stored in the Site.
     * %caption Get Bookmark
     * %ingroup Device Media
     * %struct BookmarkFilterV1
     *     %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *         field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions View bookmarks.
     * %return:{BookmarkV1} Bookmark record.
     *
     **%apidoc PUT /rest/v{1-2}/devices/{deviceId}/bookmarks/{id}
     * Replaces all fields of the particular Bookmark record stored in the Site.
     * %caption Replace Bookmark
     * %ingroup Device Media
     * %struct BookmarkV1
     *     %param[unused] creationTimeMs
     * %param[ref] _local,_strict,_ticket
     * %permissions Manage bookmarks.
     * %return:{BookmarkV1} Bookmark record.
     *
     **%apidoc PATCH /rest/v{1-2}/devices/{deviceId}/bookmarks/{id}
     * Modifies certain fields of the particular Bookmark record stored in the Site.
     * %caption Modify Bookmark
     * %ingroup Device Media
     * %struct[opt] BookmarkV1
     *     %param[unused] creationTimeMs
     *     %param[unused] serverId
     * %param[ref] _local,_strict,_ticket
     * %permissions Manage bookmarks.
     * %return:{BookmarkV1} Bookmark record.
     *
     **%apidoc DELETE /rest/v{1-2}/devices/%2A/bookmarks/{id}
     * Deletes the particular Bookmark record from the Site.
     * %caption Delete Bookmark
     * %ingroup Device Media
     * %struct BookmarkIdV1
     * %param[ref] _local
     * %param[ref] _strict,_ticket
     * %permissions Manage bookmarks.
     */
    reg("rest/v{1-2}/devices/:deviceId/bookmarks/:id?", GlobalPermission::none,
        std::make_unique<BookmarksHandler<api::BookmarkWithRuleV1>>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{3-}/devices/%2A/bookmarks
     * Retrieves the Bookmark records stored in the Site.
     * %caption Get Bookmarks
     * %ingroup Device Media
     * %struct BookmarkFilterV3
     *     %param[unused] id
     *     %param[opt] deviceId
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions View bookmarks.
     * %return:{BookmarkV3List} List of Bookmark records.
     *
     **%apidoc GET /rest/v{3-}/devices/{deviceId}/bookmarks
     * Retrieves the Bookmark records stored in the Site for the particular Device.
     * %caption Get Device Bookmarks
     * %ingroup Device Media
     * %struct BookmarkFilterV3
     *     %param[unused] id
     *     %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *         field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions View bookmarks.
     * %return:{BookmarkV3List} List of Bookmark records.
     *
     **%apidoc POST /rest/v{3-}/devices/{deviceId}/bookmarks
     * Creates a record in the Site for the new Bookmark.
     * %caption Create Bookmark
     * %ingroup Device Media
     * %struct BookmarkV3
     *     %param[unused] creationTimeMs
     *     %param[unused] id
     * %param[ref] _local,_strict,_ticket
     * %permissions Manage bookmarks.
     * %return:{BookmarkV3} Bookmark record that was created.
     *     %param[unused] password
     *
     **%apidoc GET /rest/v{3-}/devices/{deviceId}/bookmarks/{id}
     * Retrieves the particular Bookmark record stored in the Site.
     * %caption Get Bookmark
     * %ingroup Device Media
     * %struct BookmarkFilterV3
     *     %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *         field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions View bookmarks.
     * %return:{BookmarkV3} Bookmark record.
     *     %param[unused] password
     *
     **%apidoc PUT /rest/v{3-}/devices/{deviceId}/bookmarks/{id}
     * Replaces all fields of the particular Bookmark record stored in the Site.
     * %caption Replace Bookmark
     * %ingroup Device Media
     * %struct BookmarkV3
     *     %param[unused] creationTimeMs
     * %param[ref] _local,_strict,_ticket
     * %permissions Manage bookmarks.
     * %return:{BookmarkV3} Bookmark record.
     *     %param[unused] password
     *
     **%apidoc PATCH /rest/v{3-}/devices/{deviceId}/bookmarks/{id}
     * Modifies certain fields of the particular Bookmark record stored in the Site.
     * %caption Modify Bookmark
     * %ingroup Device Media
     * %struct[opt] BookmarkV3
     *     %param[unused] creationTimeMs
     * %param[ref] _local,_strict,_ticket
     * %permissions Manage bookmarks.
     * %return:{BookmarkV3} Bookmark record.
     *     %param[unused] password
     *
     **%apidoc DELETE /rest/v{3-}/devices/%2A/bookmarks/{id}
     * Deletes the particular Bookmark record from the Site.
     * %caption Delete Bookmark
     * %ingroup Device Media
     * %struct BookmarkIdV3
     * %param[ref] _local
     * %param[ref] _strict,_ticket
     * %permissions Manage bookmarks.
     */
    reg("rest/v{3-}/devices/:deviceId/bookmarks/:id?", GlobalPermission::none,
        std::make_unique<BookmarksHandler<api::BookmarkWithRuleV3>>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{1-}/devices/%2A/bookmarks/%2A/tags
     * Retrieves the Bookmark tags.
     * %caption Get Bookmark tags
     * %ingroup Device Media
     * %struct BookmarkTagFilter
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket
     * %param[ref] _local
     * %permissions View bookmarks.
     * %return:{std::map<QString, int>} All tags with Bookmark counters.
     */
    reg("rest/v{1-}/devices/*/bookmarks/*/tags", GlobalPermission::none,
        std::make_unique<BookmarkTagsHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/devices/%2A/bookmarks/{id}/description
     * Retrieves a description of the specified shared Bookmark. Does not require authorization,
     * but must require the password parameter if sharing specifies one.
     * %caption Get Bookmark Description
     * %ingroup Device Media
     * %struct BookmarkDescriptionRequest
     * %param[ref] _format,_stripDefault,_language,_local,_pretty,_strict,_ticket,_with
     * %permissions None if not password protected, Bookmark password otherwise.
     * %return:{BookmarkV3} Description of Bookmark.
     *     %param[unused] deviceId
     *     %param[unused] share
     *     %param[unused] creatorUserId
     *     %param[unused] creationTimeMs
     */
    reg("rest/v{4-}/devices/*/bookmarks/:id/description", GlobalPermission::none,
        std::make_unique<BookmarkDescriptionHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/devices/%2A/bookmarks/{bookmarkId}/media
     * Opens an HTTP video stream linked to the specified bookmark. Does not require authorization
     * if the Bookmark is shared, but must require the password parameter if sharing specifies one.
     * If required and possible, media stream transcoding is performed on-the-fly. Bookmark Media
     * Example can be found here [/ui/bookmark.html](/ui/bookmark.html).
     * %caption Bookmark HTTP Stream
     * %ingroup Device Media
     * %struct BookmarkStreamSettings
     *     %param[unused] format
     *     %param[unused] positionMs
     *     %param[unused] endPositionMs
     * %param[ref] _local,_strict,_ticket
     * %permissions None if not password protected, Bookmark password otherwise.
     * %return Media Stream.
     *
     **%apidoc GET /rest/v{4-}/devices/%2A/bookmarks/{bookmarkId}/media.{format}
     * Opens an HTTP video stream linked to the specified bookmark. Does not require authorization
     * if the Bookmark is shared, but must require the password parameter if sharing specifies one.
     * If required and possible, media stream transcoding is performed on-the-fly. Bookmark Media
     * Example can be found here [/ui/bookmark.html](/ui/bookmark.html).
     * %caption Bookmark HTTP Stream (format)
     * %ingroup Device Media
     * %struct BookmarkStreamSettings
     *     %param[unused] positionMs
     *     %param[unused] endPositionMs
     * %param[ref] _local,_strict,_ticket
     * %permissions None if not password protected, Bookmark password otherwise.
     * %return Media Stream.
     */
    reg("rest/v{4-}/devices/*/bookmarks/:bookmarkId/^media(\\.\\w+)?", GlobalPermission::none,
        std::make_unique<BookmarkStreamHandler>(authenticator, serverModule, serverConnector));

    /**%apidoc POST /rest/v1/devices/%2A/searches
     * Starts a new Device Search in the Site. The Device Search results may be obtained by the
     * GET method and are recommended to be explicitly deleted by the DELETE method after that. The
     * results are available only for a limited time after the Device Search completion.
     * %caption Start Device Search
     * %ingroup Devices
     * %struct DeviceSearchV1
     * %param[unused] id
     * %param[unused] serverId
     * %param[unused] status
     * %param[unused] devices
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{DeviceSearchV1} Device Search that was started.
     *     %param:uuid id
     *     %param[unused] serverId
     *
     **%apidoc GET /rest/v1/devices/%2A/searches/{id}
     * Retrieves information about the particular Device Search running in the Site.
     * %caption Get Device Search status
     * %ingroup Devices
     * %param:uuid id Device Search id.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{DeviceSearchV1} Device Search information.
     *     %param:uuid id
     *     %param[unused] serverId
     *
     **%apidoc GET /rest/v1/devices/%2A/searches
     * Retrieves information about all Device Searches currently running in the Site.
     * %caption Get Device Searches
     * %ingroup Devices
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{std::vector<DeviceSearchV1>} List of the Device Search information objects.
     *     %param:uuid id
     *     %param[unused] serverId
     *
     **%apidoc DELETE /rest/v1/devices/%2A/searches/{id}
     * Deletes information about the particular Device Search from the Site.
     * %caption Stop Device Search
     * %ingroup Devices
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %param:uuid id Device Search id.
     */
    reg("rest/v1/devices/*/searches/:id?", GlobalPermission::powerUser,
        std::make_unique<DeviceSearchesHandler<api::DeviceSearchV1>>(
            serverModule, serverModule->deviceSearchManager(), serverConnector));

    /**%apidoc POST /rest/v{2-3}/devices/%2A/searches
     * Starts a new Device Search in the Site. The Device Search results may be obtained by the
     * GET method and are recommended to be explicitly deleted by the DELETE method after that. The
     * results are available only for a limited time after the Device Search completion.
     * %caption Start Device Search
     * %ingroup Devices
     * %struct DeviceSearchV2
     * %param[unused] id
     * %param[unused] serverId
     * %param[unused] status
     * %param[unused] devices
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{DeviceSearchV2} Device Search that was started.
     *     %param:uuid id
     *     %param[unused] serverId
     *
     **%apidoc GET /rest/v{2-3}/devices/%2A/searches/{id}
     * Retrieves information about the particular Device Search running in the Site.
     * %caption Get Device Search status
     * %ingroup Devices
     * %param:uuid id Device Search id.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{DeviceSearchV2} Device Search information.
     *     %param:uuid id
     *     %param[unused] serverId
     *
     **%apidoc GET /rest/v{2-3}/devices/%2A/searches
     * Retrieves information about all Device Searches currently running in the Site.
     * %caption Get Device Search list
     * %ingroup Devices
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{std::vector<DeviceSearchV2>} List of Device Search information.
     *     %param:uuid id
     *     %param[unused] serverId
     *
     **%apidoc DELETE /rest/v{2-3}/devices/%2A/searches/{id}
     * Deletes information about the particular Device Search from the Site.
     * %caption Stop Device Search
     * %ingroup Devices
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %param:uuid id Device Search id.
     *
     **%apidoc POST /rest/v{4-}/devices/%2A/searches
     * Starts a new Device Search in the Site. The Device Search results may be obtained by the
     * GET method and are recommended to be explicitly deleted by the DELETE method after that. The
     * results are available only for a limited time after the Device Search completion.
     * %caption Start Device Search
     * %ingroup Devices
     * %struct DeviceSearchV2
     * %param[unused] id
     * %param[unused] status
     * %param[unused] devices
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User.
     * %return:{DeviceSearchV2} Device Search that was started.
     *
     **%apidoc GET /rest/v{4-}/devices/%2A/searches/{id}
     * Retrieves information about the particular Device Search running in the Site.
     * %caption Get Device Search status
     * %ingroup Devices
     * %param:string id Device Search id.
     * %param[ref] _local,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{DeviceSearchV2} Device Search information.
     *
     **%apidoc GET /rest/v{4-}/devices/%2A/searches
     * Retrieves information about all Device Searches currently running in the Site.
     * %caption Get Device Search list
     * %ingroup Devices
     * %param[ref] _local,_filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{std::vector<DeviceSearchV2>} List of Device Search information.
     *
     **%apidoc DELETE /rest/v{4-}/devices/%2A/searches/{id}
     * Deletes information about the particular Device Search from the Site.
     * %caption Stop Device Search
     * %ingroup Devices
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User.
     * %param:string id Device Search id.
     */
    reg("rest/v{2-}/devices/*/searches/:id?", GlobalPermission::powerUser,
        std::make_unique<DeviceSearchesHandler<api::DeviceSearchV2>>(
            serverModule, serverModule->deviceSearchManager(), serverConnector));

    /**%apidoc GET /rest/v2/devices/{id}/image
     * Retrieves a thumbnail image from the Device.
     * %caption Get Device thumbnail
     * %ingroup Device Media
     * %struct ThumbnailFilter
     * %param[ref] _language
     * %param[ref] _local,_strict,_ticket
     * %permissions View live or Archive on selected Device.
     * %return Thumbnail image.
     *
     **%apidoc GET /rest/v{3-}/devices/{id}/image
     * Retrieves a thumbnail image from the Device.
     * %caption Get Device thumbnail
     * %ingroup Device Media
     * %struct ThumbnailFilter
     * %param[unused] timestampUs
     * %param[ref] _language
     * %param[ref] _local,_strict,_ticket
     * %permissions View live or Archive on selected Device.
     * %return Thumbnail image.
     */
    reg("rest/v{2-}/devices/:id/image", GlobalPermission::none,
        std::make_unique<ThumbnailHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{3-}/devices/{id}/media
     * Opens an HTTP video stream from the Device. It is not exactly an API function but rather a
     * URL format which may contain any Device id. Live stream and archive can be downloaded. If
     * required and possible, media stream transcoding is performed on-the-fly.
     * %caption Device HTTP Stream
     * %ingroup Device Media
     * %struct MediaStreamSettings
     *     %param[unused] format
     * %param[ref] _local,_strict,_ticket
     * %permissions View live or Archive on selected Device.
     * %return Media Stream.
     *
     **%apidoc GET /rest/v{3-}/devices/{id}/media.{format}
     * Opens an HTTP video stream from the Device. It is not exactly an API function but rather a
     * URL format which may contain any Device id. Live stream and archive can be downloaded. If
     * required and possible, media stream transcoding is performed on-the-fly.
     * %caption Device HTTP Stream (format)
     * %ingroup Device Media
     * %struct MediaStreamSettings
     * %param[ref] _local,_strict,_ticket
     * %permissions View live or Archive on selected Device.
     * %return Media Stream.
     */
    reg("rest/v{3-}/devices/:id/^media(\\.\\w+)?", GlobalPermission::none,
        std::make_unique<MediaStreamHandler>(authenticator, serverModule, serverConnector));

    /**%apidoc GET /rest/v{3-}/devices/{id}/webrtc
     * Opens a WebRTC Tracker WebSocket connection. This is an implementation of the offer/answer
     * model from https://datatracker.ietf.org/doc/html/rfc3264 via websockets as the transport
     * layer. This protocol is supported by the most of modern browsers based on Chromium or Gecko
     * engines. If required and possible, media stream transcoding is performed on-the-fly.
     * <br/>
     * Example:
     * <br/>
     * <code>wss://&lt;server_ip&gt;:&lt;port&gt;/rest/v{3-}/devices/12AB42FD5912/webrtc</code>
     * <br/>
     * There is also an endpoint to a WebRTC web player provided by the Server. The endpoint can
     * be opened in a browser and can be used for embedding video steam playback into a web page.
     * The player uses the WebRTC Tracker endpoint under the hood.
     * <br/>
     * <code>https://&lt;server_ip&gt;:&lt;port&gt;/webrtc/?camera_id=12AB42FD5912&position=1674240507000000</code>
     * <br/>
     * Web player also supports all Tracker parameters:
     * <br/>
     * <code>https://&lt;server_ip&gt;:&lt;port&gt;/webrtc/?camera_id=12AB42FD5912&position=1674240507000000&stream=0&resolutionWhenTranscoding=320x240&resolution=240p&rotation=auto&speed=1.0&deliveryMethod=srtp</code>
     *
     * %caption Open WebRTC Tracker
     * %ingroup Device Media
     * %struct WebRtcTrackerSettings
     * %param[ref] _local,_strict,_ticket
     * %permissions View live or Archive on selected Device.
     * %return Video stream in WebRTC format.
     */
    reg("rest/v{3-}/devices/:id/webrtc", GlobalPermission::none,
        std::make_unique<WebRtcTrackerHandler>(authenticator, serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/devices/{id}/webrtc-camera
     * Opens a WebRTC Tracker WebSocket connection. This is an implementation of the offer/answer
     * model from https://datatracker.ietf.org/doc/html/rfc3264 via websockets as the transport
     * layer. This protocol is supported by the most of modern browsers based on Chromium or Gecko
     * engines. This Tracker is introduced for receiving video stream from WebRTC camera.
     * <br/>
     * Example:
     * </br>
     * <code>wss://&lt;server_ip&gt;:&lt;port&gt;/rest/v{4-}/devices/12AB42FD5912/webrtc-camera</code>
     * <br/>
     * There is also an endpoint to a WebRTC camera receiver provided by the Server. The endpoint can
     * be opened in a browser and can be used for streaming video from your browser into a Server.
     * The player uses the WebRTC Tracker endpoint under the hood.
     * <code>https://&lt;server_ip&gt;:&lt;port&gt;/webrtc-camera</code>
     *
     * %caption Open WebRTC Tracker
     * %ingroup Device Media
     * %struct WebRtcCameraTrackerSettings
     * %param[ref] _local,_strict,_ticket
     * %permissions Readm write and save permissions on selected Device.
     * %return None.
     */
    reg("rest/v{4-}/devices/:id/webrtc-camera", GlobalPermission::none,
        std::make_unique<WebRtcCameraTrackerHandler>(authenticator, serverModule, serverConnector));

    /**%apidoc GET /rest/v{3-}/devices/{id}/virtual/status
     * Returns virtual Device status.
     * %caption Get virtual Device status
     * %ingroup Virtual Devices
     * %struct FlexibleId
     * %param[ref] _format,_stripDefault,_language,_local,_pretty,_strict,_ticket,_with
     * %param[ref] _keepDefault
     * %permissions Depends on Resource access rights.
     * %return:{VirtualDeviceStatus} Virtual Device status.
     * %// TODO: Consider removing /extend and using /lock.
     */
    reg("rest/v{3-}/devices/:id/virtual/status",
        GlobalPermission::none,
        std::make_unique<VirtualDeviceReadStatus>(*serverModule, *serverConnector));

    /**%apidoc PATCH /rest/v{3-}/devices/{id}/virtual/prepare
     * Returns Device footage preparation result.
     * %caption Virtual Device preparation
     * %ingroup Virtual Devices
     * %struct VirtualDevicePrepare
     * %param[ref] _local,_strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{VirtualDeviceStatus} Virtual Device status.
     */
    reg("rest/v{3-}/devices/:id/virtual/prepare",
        GlobalPermission::none,
        std::make_unique<VirtualDeviceReadPrepare>(*serverModule, *serverConnector));

    /**%apidoc PATCH /rest/v{3-}/devices/{id}/virtual/consume
     * Starts a consume operation, importing already uploaded file as camera footage.
     * %caption Start Virtual Device Consume
     * %ingroup Virtual Devices
     * %struct VirtualDeviceConsume
     * %param[ref] _local,_strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{VirtualDeviceStatus} Virtual device status.
     */
    reg("rest/v{3-}/devices/:id/virtual/consume",
        GlobalPermission::none,
        std::make_unique<VirtualDeviceUpdateConsume>(*serverModule, *serverConnector));

    /**%apidoc PATCH /rest/v{3-}/devices/{id}/virtual/lock
     * Locks virtual device.
     * %caption Lock Virtual Device
     * %ingroup Virtual Devices
     * %struct VirtualDeviceLock
     * %param[ref] _local,_strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{VirtualDeviceStatus} Virtual device status.
     */
    reg("rest/v{3-}/devices/:id/virtual/lock",
        GlobalPermission::none,
        std::make_unique<VirtualDeviceUpdateLock>(*serverModule, *serverConnector));

    /**%apidoc PATCH /rest/v{3-}/devices/{id}/virtual/extend
     * Extends virtual device lock.
     * %caption Extend Virtual Device Lock
     * %ingroup Virtual Devices
     * %permissions Depends on Resource access rights.
     * %struct VirtualDeviceExtend
     * %param[ref] _local,_strict,_ticket
     * %return:{VirtualDeviceStatus} Virtual device status.
     */
    reg("rest/v{3-}/devices/:id/virtual/extend",
        GlobalPermission::none,
        std::make_unique<VirtualDeviceUpdateExtend>(*serverModule, *serverConnector));

    /**%apidoc PATCH /rest/v{3-}/devices/{id}/virtual/release
     * Releases virtual device lock.
     * %caption Release Virtual Device Lock
     * %ingroup Virtual Devices
     * %permissions Depends on Resource access rights.
     * %struct VirtualDeviceRelease
     * %param[ref] _local,_strict,_ticket
     * %return:{VirtualDeviceStatus} Virtual device status.
     */
    reg("rest/v{3-}/devices/:id/virtual/release",
        GlobalPermission::none,
        std::make_unique<VirtualDeviceUpdateRelease>(*serverModule, *serverConnector));
}

static void registerLdapRestHandlers(
    nx::network::rest::HandlerPool* processorPool,
    ldap::SyncManager* ldapSyncManager,
    auth::Authenticator* authenticator,
    nx::vms::network::AbstractServerConnector* serverConnector)
{
    const auto reg = processorPool->createRegisterHandlerFunctor();

    /**%apidoc POST /rest/v{3-}/ldap/sync
     * Synchronizes LDAP users and groups with the LDAP server.
     * %caption Synchronize LDAP
     * %ingroup LDAP
     * %param[ref] _local,_strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{LdapStatus}
     *
     **%apidoc GET /rest/v{3-}/ldap/sync
     * Tells whether the synchronization with the LDAP server is in progress.
     * %caption Get LDAP sync status
     * %ingroup LDAP
     * %param[ref] _format,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{LdapStatus}
     */
    reg("rest/v{3-}/ldap/sync", GlobalPermission::powerUser, GlobalPermission::powerUserWithFreshSession,
        std::make_unique<LdapSyncHandler>(ldapSyncManager, serverConnector));

    /**%apidoc POST /rest/v{3-}/ldap/authenticate
     * Authenticates a user on the LDAP server. The user fetched is synchronized with the Site
     * database regardless of the authentication success.
     * %caption Authenticate LDAP user
     * %ingroup LDAP
     * %struct Credentials
     * %param[ref] _local,_strict,_ticket
     * %permissions Any User.
     * %return:{UserModelV3}
     *     %param[unused] password
     *     %param[unused] resourceAccessRights
     */
    reg("rest/v{3-}/ldap/authenticate", GlobalPermission::none,
        std::make_unique<LdapAuthenticateHandler>(authenticator));

    /**%apidoc POST /rest/v{3-}/ldap/test
     * Tests the connection with the LDAP server using the settings provided.
     * %caption Test LDAP
     * %ingroup LDAP
     * %struct LdapSettings
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return
     *     %param:stringArray First user DNs found per each filter from "filters".
     */
    reg("rest/v{3-}/ldap/test", GlobalPermission::powerUser,
        std::make_unique<LdapTestHandler>(serverConnector->systemContext()));

    /**%apidoc PUT /rest/v{3-}/ldap/settings
     * %caption Set LDAP settings
     * %ingroup LDAP
     * %struct LdapSettings
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{LdapSettings}
     *     %param[unused] adminPassword
     *     %param[unused] removeRecords
     *
     **%apidoc PATCH /rest/v{3-}/ldap/settings
     * %caption Modify LDAP settings
     * %ingroup LDAP
     * %struct[opt] LdapSettings
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     * %return:{LdapSettings}
     *     %param[unused] adminPassword
     *     %param[unused] removeRecords
     *
     **%apidoc GET /rest/v{3-}/ldap/settings
     * %caption Get LDAP settings
     * %ingroup LDAP
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{LdapSettings}
     *     %param[unused] adminPassword
     *     %param[unused] removeRecords
     *
     **%apidoc DELETE /rest/v{3-}/ldap/settings
     * Deletes LDAP settings and deletes all LDAP users and groups from the VMS Site.
     * %caption Delete LDAP settings
     * %ingroup LDAP
     * %param[ref] _strict,_ticket
     * %permissions Power User with a fresh session.
     */
    reg("rest/v{3-}/ldap/settings", GlobalPermission::powerUser, GlobalPermission::powerUserWithFreshSession,
        std::make_unique<LdapSettingsHandler>(serverConnector->systemContext()));
}

static void registerIntegrationRequestRestHandlers(nx::network::rest::HandlerPool* processorPool,
    QnMediaServerModule* serverModule,
    AbstractServerConnector* serverConnector)
{
    const auto reg = processorPool->createRegisterHandlerFunctor();

    /**%apidoc GET /rest/v{4-}/integrations
     * Retrieves all Integration records stored in the Site.
     * %caption Get Integrations
     * %ingroup Site
     * %struct IntegrationInfoRequest
     * %param[unused] id
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _keepDefault
     * %permissions Power User.
     * %return:{std::vector<IntegrationInfo>} List of all Integration records.
     *
     **%apidoc GET /rest/v{4-}/integrations/{id}
     * Retrieves the specified Integration record stored in the Site.
     * %caption Get Integration
     * %ingroup Site
     * %struct IntegrationInfoRequest
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _keepDefault
     * %permissions Power User.
     * %return:{IntegrationInfo} Integration record.
     */
    reg("rest/v{4-}/integrations/:id?", GlobalPermission::none,
        std::make_unique<IntegrationsHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{4-}/analytics/integrations/%2A/requests
     * Creates a new sign up Request from an Integration. Allows an Integration to introduce
     * itself to the VMS Site. The Request will be offered for an approval by a VMS Administrator.
     * When approved, the Integration will be able to start working with the VMS Site.
     * %caption Create Integration Request
     * %ingroup Analytics
     * %struct UpdateIntegrationRequest
     * %param[unused] requestId
     * %param[ref] _strict,_ticket
     * %permissions Authorization is not required.
     * %return:{RegisterIntegrationResponse}
     *
     **%apidoc PATCH /rest/v{4-}/analytics/integrations/%2A/requests/{requestId}
     * Modifes the specified Integration Request.
     * %caption Modify Integration Request
     * %ingroup Analytics
     * %struct UpdateIntegrationRequest
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{IntegrationRequestData}
     *
     **%apidoc PUT /rest/v{4-}/analytics/integrations/%2A/requests/{requestId}
     * Replaces the entire specified Integration Request.
     * %caption Replace Integration Request
     * %ingroup Analytics
     * %struct UpdateIntegrationRequest
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     * %return:{IntegrationRequestData}
     *
     **%apidoc GET /rest/v{4-}/analytics/integrations/%2A/requests
     * Retrieves information about all Integration Requests in the Site.
     * %caption Get Integration Requests
     * %ingroup Analytics
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{std::vector<IntegrationRequestData>}
     *
     **%apidoc GET /rest/v{4-}/analytics/integrations/%2A/requests/{requestId}
     * Retrieves information about the specific Integration Request.
     * %caption Get Integration Request
     * %ingroup Analytics
     * %param:uuid requestId Integration Request id.
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Depends on Resource access rights.
     * %return:{IntegrationRequestData}
     *
     **%apidoc DELETE /rest/v{4-}/analytics/integrations/%2A/requests/{requestId}
     * Deletes the specified Integration Request.
     * %caption Delete Integration Request
     * %ingroup Analytics
     * %param:uuid requestId Integration Request id.
     * %param[ref] _strict,_ticket
     * %permissions Depends on Resource access rights.
     */
    reg("rest/v{4-}/analytics/integrations/*/requests/:requestId?",
        GlobalPermission::none,
        std::make_unique<IntegrationRequestsHandler>(serverModule));

    /**%apidoc POST /rest/v{4-}/analytics/integrations/%2A/requests/{requestId}/approve
     * Approves the specified Integration Request. Typically a Client application calls this
     * function when the VMS Administrator decides to approve the Request. After the Request is
     * approved, the Integration can learn the ids of its associated Resources using an HTTP API
     * function `GET rest/v{3-}/users/:integrationRequestId`. See the Analytics API chapter in the
     * API documentation for more information.
     * %caption Approve Integration Request
     * %ingroup Analytics
     * %param:uuid requestId Integration Request id.
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/analytics/integrations/*/requests/:requestId/approve",
        GlobalPermission::powerUser,
        std::make_unique<ApproveIntegrationRequestHandler>(serverModule));

    /**%apidoc GET /rest/v{4-}/analytics/engines/%2A/actions
     * Get Analytics Actions from all or a specified set of Analytics Plugins which are applicable
     * to the specified metadata Object type.
     * %caption Get Analytics Actions
     * %ingroup Analytics
     * %struct RequestAvailableAnalyticsActions
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{std::map<nx::Uuid, AvailableAnalyticsActions>} Map of Available Actions by Engine Id
     *
     *%apidoc GET /rest/v{4-}/analytics/engines/{engineId}/actions
     * Get Analytics Actions from all or a specified set of Analytics Plugins which are applicable
     * to the specified metadata Object type.
     * %caption Get Analytics Actions
     * %ingroup Analytics
     * %struct RequestAvailableAnalyticsActions
     * %param:string engineId
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{AvailableAnalyticsActions} Available Actions
     */
    reg("rest/v{4-}/analytics/engines/:engineId/actions",
        GlobalPermission::powerUser,
        std::make_unique<AvailableAnalyticsActionsHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{4-}/analytics/engines/{engineId}/actions/{actionId}/execute
     * Execute Analytics Action from the particular analytics Plugin on this Server. The Action is
     * applied to the specified Object Track.
     * %caption Execute Analytics Action
     * %ingroup Analytics
     * %struct AnalyticsAction
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{AnalyticsActionResult}
     */
    reg("rest/v{4-}/analytics/engines/:engineId/actions/:actionId/execute",
        GlobalPermission::powerUser,
        std::make_unique<AnalyticsEngineActionsHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/analytics/engines/{id}/settings
     * Gets settings of an Analytics Engine.
     * %caption Analytics Engine's Settings
     * %ingroup Analytics
     * %param:uuid id Engine's id
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{AnalyticsEngineSettings}
     *
     **%apidoc PUT /rest/v{4-}/analytics/engines/{id}/settings
     * Set settings for an Analytics Engine.
     * %caption Analytics Engine's Settings
     * %ingroup Analytics
     * %struct AnalyticsEngineSettings
     * %param[ref] _format,_language,_strict,_ticket
     * %permissions Power User.
     * %return:{AnalyticsEngineSettings}
     */
    reg("rest/v{4-}/analytics/engines/:id/settings",
        GlobalPermission::powerUser,
        std::make_unique<AnalyticsEngineSettingsHandler>(serverModule->systemContext()));

    /**%apidoc GET /rest/v{4-}/analytics/engines/{engineId}/deviceAgents
     * Retrieves the current state of Device Agents.
     * %caption Device Agent current states
     * %ingroup Analytics
     * %param:uuid engineId
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{std::vector<DeviceAgentState>}
     *
     **%apidoc GET /rest/v{4-}/analytics/engines/{engineId}/deviceAgents/{id}
     * Retrieves the current state of the Device Agent identified by the Engine id and Device id.
     * %caption Device Agent current state
     * %ingroup Analytics
     * %struct DeviceAgentId
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{DeviceAgentState}
     *
     **%apidoc PATCH /rest/v{4-}/analytics/engines/{engineId}/deviceAgents/{id}
     * Enables or disables the Device Agent identified by the Engine id and Device id.
     * %caption Enable/disable Device Agent
     * %ingroup Analytics
     * %struct DeviceAgentState
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{DeviceAgentState}
     */
    reg("rest/v{4-}/analytics/engines/:engineId/deviceAgents/:id?",
        GlobalPermission::powerUser,
        std::make_unique<DeviceAgentHandler>(serverModule->systemContext()));

    /**%apidoc GET /rest/v{4-}/analytics/engines/{engineId}/deviceAgents/{deviceId}/settings
     * Get settings values of the specified DeviceAgent
     * %caption Device Agent settings
     * %ingroup Analytics
     * %struct AnalyticsDeviceSettings
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{AnalyticsDeviceSettings}
     *
     **%apidoc PUT /rest/v{4-}/analytics/engines/{engineId}/deviceAgents/{deviceId}/settings
     * Set settings values of the specified DeviceAgent
     * %caption Device Agent settings
     * %ingroup Analytics
     * %struct AnalyticsDeviceSettings
     * %param[ref] _format,_language,_strict,_ticket
     * %permissions Power User.
     * %return:{AnalyticsDeviceSettings}
     */
    reg("rest/v{4-}/analytics/engines/:engineId/deviceAgents/:deviceId/settings",
        GlobalPermission::powerUser,
        std::make_unique<AnalyticsDeviceSettingsHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{3-}/analytics/engines
     * Retrieves all Analytics Engine records stored in the Site.
     * %caption Get Analytics Engines
     * %ingroup Analytics
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _keepDefault
     * %permissions Any User.
     * %return:{std::vector<EngineModel>} List of all Analytics Engine records.
     *
     **%apidoc GET /rest/v{3-}/analytics/engines/{id}
     * Retrieves the specified Analytics Engine record stored in the Site.
     * %caption Get Analytics Engine
     * %ingroup Analytics
     * %struct AnalyticsEnginesFilter
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _keepDefault
     * %permissions Any User.
     * %return:{EngineModel} Analytics Engine record.
     */
    reg("rest/v{3-}/analytics/engines/:id?", GlobalPermission::none,
        std::make_unique<AnalyticsEnginesHandler>(
            serverModule->systemContext()->resourcePool()));

    /**%apidoc GET /rest/v{3-}/analytics/integrations
     * Retrieves all Analytics Integration records stored in the Site.
     * %caption Get Analytics Integrations
     * %ingroup Analytics
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _keepDefault
     * %permissions Any User.
     * %return:{IntegrationModelList} List of all Analytics Integration records.
     *
     **%apidoc GET /rest/v{3-}/analytics/integrations/{id}
     * Retrieves the specified Analytics Integration record stored in the Site.
     * %caption Get Analytics Integration
     * %ingroup Analytics
     * %param:uuid id Analytics Integration id. Can be obtained from "id" field via
     *     `GET /rest/v{3-}/analytics/integrations`,
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _keepDefault
     * %permissions Any User.
     * %return:{IntegrationModel} Analytics Integration record.
     *
     **%apidoc DELETE /rest/v{3-}/analytics/integrations/{id}
     * Deletes the specified Integration.
     * %caption Delete Integration
     * %ingroup Analytics
     * %param:uuid id Integration id.
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{3-}/analytics/integrations/:id?", GlobalPermission::none,
        std::make_unique<AnalyticsIntegrationsHandler>(serverModule));

    /**%apidoc GET /rest/v{4-}/analytics/objectTracks
     * Searches the Analytics DB for Objects that match the specified filter, and retrieves the
     * list of the matching Object Tracks.
     * %caption Get Object Tracks
     * %ingroup Analytics
     * %struct ObjectTrackFilter
     * %param[unused] id
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{std::vector<ObjectTrackV4>} Object Tracks.
     *
     **%apidoc GET /rest/v{4-}/analytics/objectTracks/{id}
     * Retrieves the specified Object Track.
     * %caption Get Object Track
     * %ingroup Analytics
     * %struct ObjectTrackFilter
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return:{ObjectTrackV4} Object Track.
     */
    reg("rest/v{4-}/analytics/objectTracks/:id?",
        GlobalPermission::powerUser,
        std::make_unique<AnalyticsObjectTracksHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/analytics/objectTracks/{id}/bestShotImage
     * Retrieves the Best Shot image for an Analytics Object Track. The image is returned with its
     * original MIME type - either one of `image/png`, `image/jpeg`, or `image/tiff`.
     *
     * %caption Get Track Best Shot image
     * %ingroup Analytics
     * %param:uuid id Id of the Analytics Object Track.
     * %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return Best Shot image.
     *
     **%apidoc GET /rest/v{4-}/analytics/objectTracks/{id}/bestShotImage.{format}
     * Retrieves the Best Shot image for an Analytics Object Track. The image is returned with the
     * MIME type corresponding to the specified format.
     *
     * %caption Get Track Best Shot image, fmt
     * %ingroup Analytics
     * %param:uuid id Id of the Analytics Object Track.
     * %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param:enum format Defines the MIME type of the image.
     *     %value png `image/png`.
     *     %value jpg `image/jpeg`.
     *     %value tif `image/tiff`.
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return Best Shot image.
     */
    reg("rest/v{4-}/analytics/objectTracks/:id?/^bestShotImage(\\.\\w+)?",
        GlobalPermission::powerUser,
        std::make_unique<AnalyticsObjectTracksImageHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/analytics/objectTracks/{id}/objectMetadata
     * Retrieves the Metadata containing rectangles for each frame for an Analytics Object track
     * from the Archive.
     *
     * %caption Get Object Metadata
     * %ingroup Analytics
     * %struct AnalyticsObjectMetadataRequest
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{std::vector<AnalyticsObjectMetadata>} Object Metadata.
     */
    reg("rest/v{4-}/analytics/objectTracks/:id/objectMetadata",
        GlobalPermission::powerUser,
        std::make_unique<AnalyticsObjectMetadataHandler>(serverModule, serverConnector));

    /**%apidoc GET /rest/v{4-}/analytics/objectTracks/{id}/titleImage
     * Retrieves the Title image for an Analytics Object Track. The image is returned with its
     * original MIME type - either one of `image/png`, `image/jpeg`, or `image/tiff`.
     *
     * %caption Get Track Title image
     * %ingroup Analytics
     * %param:uuid id Id of the Analytics Object Track.
     * %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %permissions Power User.
     * %return Title image.
     *
     **%apidoc GET /rest/v{4-}/analytics/objectTracks/{id}/titleImage.{format}
     * Retrieves the Title image for an Analytics Object Track. The image is returned with the MIME
     * type corresponding to the specified format.
     *
     * %caption Get Track Title image, fmt
     * %ingroup Analytics
     * %param:uuid id Id of the Analytics Object Track.
     * %param:string deviceId Device id (can be obtained from "id", "physicalId" or "logicalId"
     *     field via `GET /rest/v{1-}/devices`) or MAC address (not supported for certain cameras).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %param[ref] _local
     * %param:enum format Defines the MIME type of the image.
     *     %value png `image/png`.
     *     %value jpg `image/jpeg`.
     *     %value tif `image/tiff`.
     * %permissions Power User.
     * %return Title image.
     */
    reg("rest/v{4-}/analytics/objectTracks/:id?/^titleImage(\\.\\w+)?",
        GlobalPermission::powerUser,
        std::make_unique<AnalyticsObjectTracksImageHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{4-}/analytics/engines/{id}/manifest
     * Equivalent of IEngine::pushManifest().
     * %caption Push Engine manifest
     * %ingroup Analytics
     * %struct PushEngineManifestData
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/analytics/engines/:id/manifest",
        GlobalPermission::powerUser,
        std::make_unique<PushEngineManifestHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{4-}/analytics/engines/{id}/deviceAgents/{deviceId}/manifest
     * Equivalent of IDeviceAgent::pushManifest().
     * %caption Push Device Agent Manifest
     * %ingroup Analytics
     * %struct PushDeviceAgentManifestData
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/analytics/engines/:id/deviceAgents/:deviceId/manifest",
        GlobalPermission::powerUser,
        std::make_unique<PushDeviceAgentManifestHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{4-}/analytics/engines/{id}/integrationDiagnosticEvent
     * Equivalent of IEngine::pushIntegrationDiagnosticEvent().
     * %caption Push Diagnostic Event
     * %ingroup Analytics
     * %struct EngineIntegrationDiagnosticEvent
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/analytics/engines/:id/integrationDiagnosticEvent",
        GlobalPermission::powerUser,
        std::make_unique<EngineIntegrationDiagnosticEventHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{4-}/analytics/engines/{id}/deviceAgents/{deviceId}/integrationDiagnosticEvent
     * Equivalent of IDeviceAgent::pushIntegrationDiagnosticEvent().
     * %caption Push Diagnostic Event
     * %ingroup Analytics
     * %struct DeviceAgentIntegrationDiagnosticEvent
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/analytics/engines/:id/deviceAgents/:deviceId/integrationDiagnosticEvent",
        GlobalPermission::powerUser,
        std::make_unique<DeviceAgentIntegrationDiagnosticEventHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{4-}/analytics/engines/{id}/deviceAgents/{deviceId}/metadata/object
     * Equivalent of IDeviceAgent::IHandler::handleMetadata().
     * %caption Push Object metadata
     * %ingroup Analytics
     * %struct ApiObjectMetadataPacket
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/analytics/engines/:id/deviceAgents/:deviceId/metadata/object",
        GlobalPermission::powerUser,
        std::make_unique<DeviceAgentObjectMetadataHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{4-}/analytics/engines/{id}/deviceAgents/{deviceId}/metadata/event
     * Equivalent of IDeviceAgent::IHandler::handleMetadata().
     * %caption Push Event metadata
     * %ingroup Analytics
     * %struct ApiEventMetadataPacket
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/analytics/engines/:id/deviceAgents/:deviceId/metadata/event",
        GlobalPermission::powerUser,
        std::make_unique<DeviceAgentEventMetadataHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{4-}/analytics/engines/{id}/deviceAgents/{deviceId}/metadata/bestShot
     * Equivalent of IDeviceAgent::IHandler::handleMetadata().
     * %caption Push Best Shot metadata
     * %ingroup Analytics
     * %struct ApiBestShotMetadataPacket
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/analytics/engines/:id/deviceAgents/:deviceId/metadata/bestShot",
        GlobalPermission::powerUser,
        std::make_unique<DeviceAgentBestShotMetadataHandler>(serverModule, serverConnector));

    /**%apidoc POST /rest/v{4-}/analytics/engines/{id}/deviceAgents/{deviceId}/metadata/title
     * Equivalent of IDeviceAgent::IHandler::handleMetadata().
     * %caption Push Title metadata
     * %ingroup Analytics
     * %struct ApiObjectTitleMetadataPacket
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/analytics/engines/:id/deviceAgents/:deviceId/metadata/title",
        GlobalPermission::powerUser,
        std::make_unique<DeviceAgentObjectTitleMetadataHandler>(serverModule, serverConnector));
}

static void registerMiscRestHandlers(
    nx::network::rest::HandlerPool* processorPool, QnMediaServerModule* serverModule)
{
    const auto reg = processorPool->createRegisterHandlerFunctor();
    const auto* systemContext = serverModule->systemContext();

    /**%apidoc GET /rest/v{1-2}/licenseSummaries
     * %caption Get license usage info
     * %ingroup Licenses
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{std::map<QString, LicenseSummaryData>} License summary information
     */
    reg("rest/v{1-2}/licenseSummaries", GlobalPermission::powerUser,
        std::make_unique<LicenseSummaryHandler>(serverModule));

    /**%apidoc GET /rest/v{3-}/licenses/%2A/summary
     * %caption Get license usage info
     * %ingroup Licenses
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{std::map<QString, LicenseSummaryData>} License summary information.
     */
    reg("rest/v{3-}/licenses/*/summary", GlobalPermission::powerUser,
        std::make_unique<LicenseSummaryHandler>(serverModule));

    /**%apidoc:{std::variant<std::vector<Request>, Request>} POST /jsonrpc
     * Allows executing API requests in the JSON RPC format over HTTP. For more information see the
     * JSON-RPC section in the API Information page.
     * %caption JSON-RPC (HTTP)
     * %ingroup Utilities
     * %param[ref] _ticket
     * %permissions Depends on Resource access rights.
     * %return:{std::variant<std::vector<Response>, Response>}
     *
     **%apidoc GET /jsonrpc
     * Allows executing API requests in the JSON RPC format over a WebSocket. For more information
     * see the JSON-RPC section in the API Information page. JSON-RPC WebSocket Example can be
     * found here [/ui/jsonrpc.html](/ui/jsonrpc.html).
     * %caption JSON-RPC (WebSocket)
     * %ingroup Utilities
     * %param[ref] _ticket
     * %permissions Depends on Resource access rights.
     * %return Response with "switching protocols" HTTP status code.
     *
     **%apidoc OPTIONS /jsonrpc
     * Allows executing API requests in the JSON RPC format over a WebSocket. For more information
     * see the JSON-RPC section in the API Information page. JSON-RPC WebSocket Example can be found
     * here [/ui/jsonrpc.html](/ui/jsonrpc.html).
     * %caption JSON-RPC (WebSocket options)
     * %ingroup Utilities
     * %param[ref] _ticket
     * %permissions Depends on Resource access rights.
     * %return Response with the "switching protocols" HTTP status code.
     */
    reg("jsonrpc", GlobalPermission::none,
        std::make_unique<JsonRpcHandler>(systemContext, processorPool),
        nx::network::http::Method::post);

    /**%apidoc GET /rest/v{3-}/users/{id}/permissions
     * %caption Get User Permissions
     * %ingroup Users
     * %param:string id User id or name (can be obtained from `/rest/v{3-}/users`). Use "-" for the
     *     Current User.
     *     %example -
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions All Users can see own permissions, Power Users can see everything.
     * %return:{PermissionsModel} Inherited groups and effective permissions.
     */
    reg("rest/v{3-}/users/:id/permissions", GlobalPermission::none,
        std::make_unique<UserPermissionsHandler>(
            systemContext->resourceAccessManager(), systemContext->userGroupManager(),
            systemContext->resourcePool()));

    /**%apidoc GET /rest/v{3-}/userGroups/{id}/permissions
     * %caption Get User Group Permissions
     * %ingroup User Groups
     * %param:string id User Group id (can be obtained from `/rest/v{3-}/userGroups`).
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{PermissionsModel} Inherited groups and effective permissions.
     */
    reg("rest/v{3-}/userGroups/:id/permissions", GlobalPermission::powerUser,
        std::make_unique<UserGroupsPermissionsHandler>(
            systemContext->resourceAccessManager(),
            systemContext->userGroupManager()));
}

static void registerExperimentalHandlers(
    nx::network::rest::HandlerPool* processorPool,
    QnMediaServerModule* serverModule)
{
    const auto reg = processorPool->createRegisterHandlerFunctor();

    /**%apidoc[proprietary] GET /rest/v{2-}/experimental/cameras
     * %caption Get camera list
     * %ingroup Devices
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     */
    reg("rest/v{2-}/experimental/cameras", GlobalPermission::powerUser,
        std::make_unique<experimental::CamerasListHandler>(serverModule->systemContext()));
}

static void registerUpdateHandlers(
    nx::network::rest::HandlerPool* processorPool,
    QnMediaServerModule* serverModule)
{
    const auto reg = processorPool->createRegisterHandlerFunctor();

    /**%apidoc POST /rest/v{4-}/update/start
     * Starts an update process. Input to this can be requested from /rest/v4/update/info
     * %caption Start Update
     * %ingroup Update
     * %struct UpdateInformation
     * %permissions Power User.
     * %param[ref] _strict,_ticket
     * %return:{StartUpdateReply}
     */
    reg("rest/v{4-}/update/start", GlobalPermission::powerUserWithFreshSession,
        std::make_unique<StartUpdateHandler>(serverModule));

    /**%apidoc POST /rest/v{4-}/update/install
     * Initiates update package installation.
     * %caption Install Update
     * %ingroup Update
     * %struct InstallUpdateRequest
     * %permissions Power User.
     * %param[ref] _strict,_ticket
     */
    reg("rest/v{4-}/update/install", GlobalPermission::powerUserWithFreshSession,
        std::make_unique<InstallUpdateHandler>(serverModule));

    /**%apidoc POST /rest/v{4-}/update/finish
     * Puts a Site in the 'Update Finished' state.
     * %caption Finish Update
     * %ingroup Update
     * %struct FinishUpdateRequest
     * %permissions Power User.
     * %param[ref] _strict,_ticket
     */
    reg("rest/v{4-}/update/finish", GlobalPermission::powerUserWithFreshSession,
        std::make_unique<FinishUpdateHandler>(serverModule));

    /**%apidoc POST /rest/v{4-}/update/retry
     * Retries the latest failed update action. E.g. if one of servers has failed update because
     * there was not enough free space, it will retry to reserve space and start downloading.
     * %caption Retry Update
     * %ingroup Update
     * %permissions Power User.
     * %param[ref] _strict,_ticket
     * %return:{std::map<nx::Uuid, UpdateStatusInfo>}
     */
    reg("rest/v{4-}/update/retry", GlobalPermission::powerUserWithFreshSession,
        std::make_unique<RetryUpdateHandler>(serverModule));

    /**%apidoc GET /rest/v{4-}/update/info
     * Retrieves a currently present or specified via a parameter update information manifest.
     * %caption Update Information
     * %ingroup Update
     * %struct UpdateInfoRequest
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket
     * %param[ref] _with,_local
     * %permissions Any User.
     * %return:{UpdateInformation} Update Information.
     */
    reg("rest/v{4-}/update/info", nx::vms::api::GlobalPermission::none,
        std::make_unique<UpdateInfoHandler>(serverModule));

    /**%apidoc GET /rest/v{4-}/update
     * Retrieves the current update processing Site-wide state.
     * %caption Get Update Status
     * %ingroup Update
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{std::map<nx::Uuid, UpdateStatusInfo>}
     *
     * %apidoc DELETE /rest/v{4-}/update
     * Puts a Site in the 'Idle' update state. The current update manifest will be cleared and all
     * downloads will be cancelled.
     * %caption Cancel Update
     * %ingroup Update
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     */
    reg("rest/v{4-}/update", GlobalPermission::powerUser,
        std::make_unique<UpdateHandler>(serverModule));

    /**%apidoc POST /rest/v{4-}/update/storage
     * Set a list of Server ids used for persistent update file storage.
     * %caption Set Server Persistent Storage
     * %ingroup Update
     * %struct PersistentUpdateStorageInfo
     * %param[ref] _strict,_ticket
     * %permissions Power User.
     * %return:{PersistentUpdateStorageInfo}
     *
     * %apidoc GET /rest/v{4-}/update/storage/{infoCategory}
     * Retrieves a currently present list of Server ids used for update file storage.
     * %caption Get Persistent Storage Servers
     * %ingroup Update
     * %struct PersistentUpdateStorageRequest
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Power User.
     * %return:{PersistentUpdateStorageInfo}
     */
    reg("rest/v{4-}/update/storage/:infoCategory?", GlobalPermission::powerUser,
        std::make_unique<UpdateStorageHandler>(serverModule));
}
