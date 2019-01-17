#pragma once

#include <future>

#include <QtCore>
#include <QString>

#include <nx/update/update_information.h>
#include <nx/vms/api/data/system_information.h>
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
    latestUpdateInstalled
};

Information updateInformation(
    const QString& url,
    const QString& publicationKey = kLatestVersion,
    InformationError* error = nullptr);

FindPackageResult findPackage(
    const vms::api::SystemInformation& systemInformation,
    const nx::update::Information& updateInformation,
    bool isClient,
    const QString& cloudHost,
    bool boundToCloud,
    nx::update::Package* outPackage,
    QString* outMessage);

FindPackageResult findPackage(
    const vms::api::SystemInformation& systemInformation,
    const QByteArray& serializedUpdateInformation,
    bool isClient,
    const QString& cloudHost,
    bool boundToCloud,
    nx::update::Package* outPackage,
    QString* outMessage);

/**
 * Simple search for a package
 * @param component component string: {server, client}
 * @param systemInfo system info of package being searched
 * @param updateInfo update contents
 * @return first package with specified data, or nullptr if no package is found. Note: it points
 *     to the object inside updateInfo. So be careful with the lifetime of updateInfo.
 */
nx::update::Package* findPackage(
    const QString& component,
    nx::vms::api::SystemInformation& systemInfo,
    nx::update::Information& updateInfo);

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
    const QString& build,
    UpdateCheckCallback&& callback = {});

} // namespace nx::update
