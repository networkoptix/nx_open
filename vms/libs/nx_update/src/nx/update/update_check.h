#pragma once

#include <future>
#include <utility>

#include <QtCore/QString>

#include <nx/update/update_information.h>
#include <nx/utils/os_info.h>
#include <common/common_module.h>

namespace nx::update {

const static QString kLatestVersion = "latest";
const static QString kComponentClient = "client";
const static QString kComponentServer = "server";

enum class FindPackageResult
{
    ok,
    noInfo,
    otherError,
    latestUpdateInstalled,
    notParticipant,
    osVersionNotSupported,
};

/**
 * Searches update information on the update server by the publicationKey (version).
 * @param url an update server url
 * @param currentVersion a version to compare against in case if the publicationKey == "latest".
 *     Refer to #dklychkov or #dkargin for details
 * @param publicationKey a version id to use while searching
 * @param error an out parameter used to report encountered errors
 */
Information updateInformation(
    const QString& url,
    const nx::vms::api::SoftwareVersion& currentVersion,
    const QString& publicationKey = kLatestVersion,
    InformationError* error = nullptr);

/** Searches for an update package for a specified OS variant. */
std::pair<FindPackageResult, const Package*> findPackageForVariant(
    const nx::update::Information& updateInformation,
    bool isClient,
    const utils::OsInfo& osInfo);
std::pair<FindPackageResult, Package*> findPackageForVariant(
    nx::update::Information& updateInformation,
    bool isClient,
    const utils::OsInfo& osInfo);

/**
 * Searches for an update package for your particular module in the global update information.
 * @param commonModule a reference to the QnCommonModuleInstance
 * @param outPacakge a result package will be placed here, if found
 * @param outMessage a detailed error message
 */
FindPackageResult findPackage(
    const QnCommonModule& commonModule,
    nx::update::Package* outPackage,
    QString* outMessage);

/**
 * Searches for an update package for your particular module in the given update information.
 */
FindPackageResult findPackage(
    const QnCommonModule& commonModule,
    const Information& updateInformation,
    nx::update::Package* outPackage,
    QString* outMessage);

/**
 * A wrapper for QJson::deserialize(). Processes common errors and fills the error message
 * accordingly.
 */
FindPackageResult fromByteArray(
    const QByteArray& serializedUpdateInformation,
    Information* outInformation,
    QString* outMessage);

using UpdateCheckCallback = std::function<void (const UpdateContents&)>;

/**
 * Checks update for specific build from the internet. It starts asynchronously.
 * @param updateUrl Url to update server. It should lead directly to updates.json file,
 *     like http://vms_updates:8080/updates/updates.json.
 * @param callback Callback to be called when update info is obtained.
 * @return Future object to be checked for completion.
 */
std::future<UpdateContents> checkLatestUpdate(
    const QString& updateUrl,
    const nx::vms::api::SoftwareVersion& engineVersion,
    UpdateCheckCallback&& callback = {});

/**
 * Checks update for specific build from the internet. It starts asynchronously.
 * @param updateUrl Url to update server. It should lead directly to updates.json file,
 *     like http://vms_updates:8080/updates/updates.json.
 * @param build Build number, like "28057".
 * @param callback Callback to be called when update info is obtained.
 * @return Future object to be checked for completion.
 */
std::future<UpdateContents> checkSpecificChangeset(
    const QString& updateUrl,
    const nx::vms::api::SoftwareVersion& engineVersion,
    const QString& build,
    UpdateCheckCallback&& callback = {});

QString rootUpdatesDirectoryForDownloader();
QString updatesDirectoryForDownloader(const QString& publicationKey);
QString updateFilePathForDownloader(const QString& publicationKey, const QString& fileName);

} // namespace nx::update
