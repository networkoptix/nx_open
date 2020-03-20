#pragma once

#include <set>

#include <nx/vms/api/data/camera_data_ex.h>
#include <nx/vms/api/data/client_info_data.h>
#include <nx/vms/api/data/event_rule_data.h>
#include <nx/vms/api/data/layout_data.h>
#include <nx/vms/api/data/license_data.h>
#include <nx/vms/api/data/media_server_data.h>
#include <nx/vms/api/data/resource_data.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/api/data/videowall_data.h>
#include <nx/vms/api/data/analytics_data.h>

namespace nx::vms::api {

/**
 * NOTE: Many of the structures below are created with a single purpose - to filter out the fields
 * containing sensitive information the User could be identified by and make the report completely
 * anonymous.
 */

/**%apidoc
 * Information about an analytics object/event type.
 */
struct NX_VMS_API DeviceAnalyticsTypeInfo
{
    /**%apidoc
     * Id of object/event type.
     */
    QString id;

    /**%apidoc
     *  Name of object/event type.
     */
    QString name;

    /**%apidoc
     * Name of vendor that provides this object/event type. Empty string if unknown.
     */
    QString provider;
};
#define DeviceAnalyticsTypeInfo_Fields (id)(name)(provider)

/**%apidoc
 * Statistics information about analytics object/events supported by the System.
 */
struct NX_VMS_API DeviceAnalyticsStatistics
{
    /**%apidoc
     * Analytics event types supported by the System.
     */
    std::vector<DeviceAnalyticsTypeInfo> supportedEventTypes;

    /**%apidoc
     * Analytics object types supported by the System.
     */
    std::vector<DeviceAnalyticsTypeInfo> supportedObjectTypes;
};
#define DeviceAnalyticsStatistics_Fields (supportedEventTypes)(supportedObjectTypes)

/**%apidoc
 * Anonymous statistics data about Devices in the System.
 */
struct NX_VMS_API StatisticsCameraData: public nx::vms::api::CameraDataEx
{
    /**%apidoc
     * Statistics information about the analytics subsystem.
     */
    DeviceAnalyticsStatistics analyticsInfo;
};
#define StatisticsCameraData_Fields \
    (id) \
    (parentId) \
    (status) \
    (addParams) \
    (manuallyAdded) \
    (model) \
    (statusFlags) \
    (vendor) \
    (scheduleEnabled) \
    (motionType) \
    (motionMask) \
    (scheduleTasks) \
    (audioEnabled) \
    (disableDualStreaming) \
    (controlEnabled) \
    (dewarpingParams) \
    (minArchiveDays) \
    (maxArchiveDays) \
    (preferredServerId) \
    (backupType) \
    (analyticsInfo)

/**%apidoc
 * Anonymous statistics data about Storages in the System.
 */
struct NX_VMS_API StatisticsStorageData: public nx::vms::api::StorageData
{
};
#define StatisticsStorageData_Fields \
    (id) \
    (parentId) \
    (spaceLimit) \
    (usedForWriting) \
    (storageType) \
    (isBackup) \
    (addParams)

/**%apidoc
 * Anonymous statistics data about device, storage and analytics Plugins in the System.
 */
struct NX_VMS_API StatisticsPluginInfo: public nx::vms::api::ExtendedPluginInfo
{
};
#define StatisticsPluginInfo_Fields \
    (name) \
    (version) \
    (vendor) \
    (optionality) \
    (status) \
    (errorCode) \
    (mainInterface) \
    (resourceBindingInfo)

/**%apidoc
 * Anonymous statistics data about Servers in the System.
 */
struct NX_VMS_API StatisticsMediaServerData: public nx::vms::api::MediaServerDataEx
{
    /**%apidoc
     * Anonymous statistics data about Storages on the Server.
     */
    std::vector<StatisticsStorageData> storages; //< Overrides the base class field.

    /**%apidoc
     * Anonymous statistics data about Plugins on the Server.
     */
    std::vector<StatisticsPluginInfo> pluginInfo;
};
#define StatisticsMediaServerData_Fields \
    (id) \
    (parentId) \
    (status) \
    (storages) \
    (addParams) \
    (flags) \
    (version) \
    (systemInfo) \
    (maxCameras) \
    (allowAutoRedundancy) \
    (backupType) \
    (backupDaysOfTheWeek) \
    (backupStart) \
    (backupDuration) \
    (backupBitrate) \
    (pluginInfo) \

/**%apidoc
 * Anonymous statistics data about licenses in the System.
 */
struct NX_VMS_API StatisticsLicenseData
{
    StatisticsLicenseData() = default;
    StatisticsLicenseData(const nx::vms::api::LicenseData& data);

    QString name;
    QString key;
    QString licenseType;
    QString version;
    QString brand;
    QString expiration;
    QString validation;
    qint64 cameraCount = 0;
};
#define StatisticsLicenseData_Fields \
    (name) \
    (key) \
    (cameraCount) \
    (licenseType) \
    (version) \
    (brand) \
    (expiration) \
    (validation)

/**%apidoc
 * Anonymous statistics data about event rules in the System.
 */
struct NX_VMS_API StatisticsEventRuleData: public nx::vms::api::EventRuleData
{
};
#define StatisticsEventRuleData_Fields \
    (id) \
    (eventType) \
    (eventResourceIds) \
    (eventCondition) \
    (eventState) \
    (actionType) \
    (actionResourceIds) \
    (actionParams) \
    (aggregationPeriod) \
    (disabled) \
    (schedule) \
    (system)

/**%apidoc
 * Anonymous statistics data about Users in the System.
 */
struct NX_VMS_API StatisticsUserData: public nx::vms::api::UserData
{
};
#define StatisticsUserData_Fields (id)(isAdmin)(permissions)(isLdap)(isEnabled)

/**%apidoc
 * Information about the statistics report itself.
 */
struct NX_VMS_API StatisticsReportInfo
{
    StatisticsReportInfo():
        id(QnUuid::createUuid())
    {
    }

    QnUuid id;
    qint64 number = -1;
};
#define StatisticsReportInfo_Fields (id)(number)

/**%apidoc
 * Anonymous statistics report containing information about different System aspects.
 */
struct NX_VMS_API SystemStatistics
{
    QnUuid systemId;
    StatisticsReportInfo reportInfo;

    std::vector<StatisticsEventRuleData> businessRules;
    std::vector<StatisticsCameraData> cameras;
    std::vector<StatisticsLicenseData> licenses;
    std::vector<StatisticsMediaServerData> mediaservers;
    nx::vms::api::LayoutDataList layouts;
    std::vector<StatisticsUserData> users;
    nx::vms::api::VideowallDataList videowalls;
};
#define SystemStatistics_Fields \
    (systemId) \
    (mediaservers) \
    (cameras) \
    (licenses) \
    (businessRules) \
    (layouts) \
    (users) \
    (videowalls) \
    (reportInfo)

/**%apidoc
 * Parameters for the statistics report generation procedure.
 */
struct NX_VMS_API StatisticsServerArguments
{
    bool randomSystemId = false;
};
#define StatisticsServerArguments_Fields (randomSystemId)

/**%apidoc
 * Information about the current statistics delivery procedure.
 */
struct NX_VMS_API StatisticsServerInfo
{
    /**%apidoc
     * Id of the System.
     */
    QnUuid systemId;

    /**%apidoc
     * Url of the statistics Server the report is going to be delivered to.
     */
    QString url;

    /**%apidoc
     * Current status of the statistics delivery procedure.
     */
    QString status;
};
#define StatisticsServerInfo_Fields (systemId)(url)(status)

#define STATISTICS_DATA_TYPES \
    (StatisticsCameraData) \
    (StatisticsStorageData) \
    (StatisticsMediaServerData) \
    (StatisticsLicenseData) \
    (StatisticsEventRuleData) \
    (StatisticsUserData) \
    (StatisticsPluginInfo) \
    (StatisticsReportInfo) \
    (SystemStatistics) \
    (StatisticsServerArguments) \
    (StatisticsServerInfo) \
    (DeviceAnalyticsTypeInfo) \
    (DeviceAnalyticsStatistics)

} // namespace nx::vms::api
