// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**@file
 * Intended to be included into the Server code.
 */

void registerSiteSettingHandlers(
    nx::network::rest::HandlerPool* processorPool,
    QnMediaServerModule* serverModule,
    nx::vms::cloud_integration::CloudManagerGroup* cloudManagerGroup)
{
    const auto reg = processorPool->createRegisterHandlerFunctor();

    /**%apidoc POST /rest/v{1-2}/system/cloudBind
     * Binds the Site to the Cloud.
     * %caption Bind Site to Cloud
     * %ingroup Site
     * %struct CloudSystemAuth
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     */
    reg("rest/v{1-2}/system/cloudBind", GlobalPermission::administratorWithFreshSession,
        std::make_unique<CloudBindHandler<api::CloudSystemAuth>>(
            serverModule, cloudManagerGroup));

    /**%apidoc POST /rest/v3/system/cloud/bind
     * Binds the Site to the Cloud.
     * %caption Bind Site to Cloud
     * %ingroup Site
     * %struct CloudSystemAuth
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     */
    reg("rest/v3/system/cloud/bind", GlobalPermission::administratorWithFreshSession,
        std::make_unique<CloudBindHandler<api::CloudSystemAuth>>(
            serverModule, cloudManagerGroup));

    /**%apidoc POST /rest/v{4-}/cloud/bind
     * Binds the Site to the Cloud.
     * %caption Bind Site to Cloud
     * %ingroup Cloud
     * %struct CloudSiteAuth
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     */
    reg("rest/v{4-}/cloud/bind", GlobalPermission::administratorWithFreshSession,
        std::make_unique<CloudBindHandler<api::CloudSiteAuth>>(
            serverModule, cloudManagerGroup));

    /**%apidoc POST /rest/v{1-2}/system/cloudUnbind
     * Unbinds the Site from the Cloud and sets a new administrator password.
     * %caption Unbind Site from Cloud
     * %ingroup Site
     * %struct LocalSiteAuth
     * %param[opt] password
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     */
    reg("rest/v{1-2}/system/cloudUnbind", GlobalPermission::administratorWithFreshSession,
        std::make_unique<CloudUnbindHandler>(serverModule, cloudManagerGroup));

    /**%apidoc POST /rest/v3/system/cloud/unbind
     * Unbinds the Site from the Cloud and sets a new administrator password.
     * %caption Unbind Site from Cloud
     * %ingroup Site
     * %struct LocalSiteAuth
     * %param[opt] password
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     */
    reg("rest/v3/system/cloud/unbind", GlobalPermission::administratorWithFreshSession,
        std::make_unique<CloudUnbindHandler>(serverModule, cloudManagerGroup));

    /**%apidoc POST /rest/v{4-}/cloud/unbind
     * Unbinds the Site from the Cloud and sets a new administrator password.
     * %caption Unbind Site from Cloud
     * %ingroup Cloud
     * %struct LocalSiteAuth
     * %param[opt] password
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     */
    reg("rest/v{4-}/cloud/unbind", GlobalPermission::administratorWithFreshSession,
        std::make_unique<CloudUnbindHandler>(serverModule, cloudManagerGroup));

    /**%apidoc POST /rest/v{1-3}/system/setup
     * Sets up this Server to form a new Site. Can only be called if this Server is not a part of
     * any other Site.
     * %caption Set up Site
     * %ingroup Site
     * %struct SetupSystemData
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     */
    reg("rest/v{1-3}/system/setup", GlobalPermission::administratorWithFreshSession,
        std::make_unique<SiteSetupHandler<api::SetupSystemData>>(
            serverModule, cloudManagerGroup));

    /**%apidoc POST /rest/v{4-}/site/setup
     * Sets up this Server to form a new Site. Can only be called if this Server is not a part of
     * any other Site.
     * %caption Set up Site
     * %ingroup Site
     * %struct SetupSiteData
     *     %param:{std::variant<int, QString>} settings.auditTrailPeriodDays
     *     %param:{std::variant<int, QString>} settings.ec2AliveUpdateIntervalSec
     *     %param:{std::variant<int, QString>} settings.eventLogPeriodDays
     *     %param:{std::variant<int, QString>} settings.maxDifferenceBetweenSynchronizedAndInternetTime
     *     %param:{std::variant<int, QString>} settings.maxRtspConnectDurationSeconds
     *     %param:{std::variant<int, QString>} settings.syncTimeEpsilon
     *     %param:{std::variant<int, QString>} settings.syncTimeExchangePeriod
     * %param[ref] _strict,_ticket
     * %permissions Administrator with a fresh session.
     */
    reg("rest/v{4-}/site/setup", GlobalPermission::administratorWithFreshSession,
        std::make_unique<SiteSetupHandler<api::SetupSiteData>>(serverModule, cloudManagerGroup));

    /**%apidoc GET /rest/v{1-3}/system/settings
     * %caption Get Site settings
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return Site settings and their values.
     *
     **%apidoc GET /rest/v{1-3}/system/settings/{name}
     * %caption Get Site setting
     * %ingroup Site
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket
     * %param:string name Site setting name to retrieve.
     *     %example systemName
     * %permissions Any User.
     * %return Value of the particular Site setting.
     *
     **%apidoc:object PATCH /rest/v{1-3}/system/settings
     * Modifies a bunch of Site settings by passing a JSON object with the setting keys and
     * values in the request body. The existing settings can be obtained in the same format using
     * the "Get Site settings" function. The complete list of settings with their types is
     * returned by the "Get Site settings manifest" function.
     * %example {"systemName": "Site"}
     * %caption Modify Site settings
     * %ingroup Site
     * %param[ref] _strict,_ticket
     * %permissions Administrator or power user (see manifest) with a fresh session.
     *
     **%apidoc:any PUT /rest/v{1-3}/system/settings/{name}
     * Replaces a single Site setting by passing a JSON value in the request body. The existing
     * setting values can be obtained using the "Get Site settings" function as a JSON object
     * with setting values mapped by field names. The complete list of settings with their types is
     * returned by the "Get Site settings manifest" function.
     * %example Site
     * %caption Replace Site setting
     * %ingroup Site
     * %param[ref] _strict,_ticket
     * %param:string name Site setting name to replace.
     *     %example systemName
     * %permissions Administrator or power user (see manifest) with a fresh session.
     */
    reg("rest/v{1-3}/system/settings/:name?",
        GlobalPermission::none, GlobalPermission::powerUserWithFreshSession,
        std::make_unique<SystemSettingsHandler>(serverModule->systemContext(), cloudManagerGroup));

    /**%apidoc GET /rest/v{4-}/site/settings
     * %caption Get Site settings
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{SiteSettings} Site settings and their values.
     *
     **%apidoc GET /rest/v{4-}/site/settings/{name}
     * %caption Get Site setting
     * %ingroup Site
     * %param[ref] _format,_stripDefault,_language,_pretty,_strict,_ticket
     * %param:string name Site setting name to retrieve.
     *     %example siteName
     * %permissions Any User.
     * %return Value of the particular Site setting.
     *
     **%apidoc PATCH /rest/v{4-}/site/settings
     * Modifies a bunch of Site settings by passing a JSON object with the setting keys and
     * values in the request body. The existing settings can be obtained in the same format using
     * the "Get Site settings" function. The complete list of settings with their types is
     * returned by the "Get Site settings manifest" function.
     * %example {"siteName": "Site"}
     * %caption Modify Site settings
     * %ingroup Site
     * %struct[opt] SaveableSiteSettings
     * %param[ref] _strict,_ticket
     * %permissions Administrator or power user (see manifest) with a fresh session.
     * %return:{SiteSettings}
     *
     **%apidoc:any PUT /rest/v{4-}/site/settings/{name}
     * Replaces a single Site setting by passing a JSON value in the request body. The existing
     * setting values can be obtained using the "Get Site settings" function as a JSON object
     * with setting values mapped by field names. The complete list of settings with their types is
     * returned by the "Get Site settings manifest" function.
     * %example Site
     * %caption Replace Site setting
     * %ingroup Site
     * %param[ref] _strict,_ticket
     * %param:string name Site setting name to replace.
     *     %example siteName
     * %permissions Administrator or power user (see manifest) with a fresh session.
     */
    reg("rest/v{4-}/site/settings/:name?",
        GlobalPermission::none, GlobalPermission::powerUserWithFreshSession,
        std::make_unique<SiteSettingsHandler>(serverModule->systemContext(), cloudManagerGroup));

    /**%apidoc GET /rest/v1/system/settingsManifest
     * %caption Get Site settings manifest
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{std::map<QString, SystemSettingDescription>} Descriptions of the Site settings.
     */
    reg("rest/v1/system/settingsManifest", GlobalPermission::none,
        std::make_unique<SystemSettingsManifestRestHandlerV1>(serverModule->globalSettings()));

    /**%apidoc GET /rest/v{2-3}/system/settings/%2A/manifest
     * %caption Get Site settings manifest
     * %ingroup Site
     * %param[ref] _filter,_format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket,_with
     * %permissions Any User.
     * %return:{std::map<QString, SystemSettingDescription>} Descriptions of the Site settings.
     *
     * %apidoc GET /rest/v{2-3}/system/settings/{name}/manifest
     * %caption Get Site setting manifest
     * %ingroup Site
     * %param[ref] _format,_stripDefault,_keepDefault,_language,_pretty,_strict,_ticket
     * %param:string name Site setting name to get.
     *     %example systemName
     * %permissions Any User.
     * %return:{SystemSettingDescription} Description of the particular Site setting.
     */
    reg("rest/v{2-3}/system/settings/:name/manifest", GlobalPermission::none,
        std::make_unique<SystemSettingsManifestRestHandlerV2>(serverModule->globalSettings()));
}
