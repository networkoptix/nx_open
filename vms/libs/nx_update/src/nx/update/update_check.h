#pragma once

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
const nx::update::Package* findPackage(
    const QString& component,
    nx::vms::api::SystemInformation& systemInfo,
    const nx::update::Information& updateInfo);

/**
 * We use this class when regular polling for std::future<UpdateInformation> is not convenient.
 */
class UpdateCheckNotifier: public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

signals:
    void finished(const nx::update::UpdateContents& contents);
};

/**
 * Checks update from the internet.
 * It starts asynchronously. If signaller is specified, it will emit notifier->atFinished.
 * @param updateUrl Url to update server. It should lead directly to updates.json file,
 *     like http://vms_updates:8080/updates/updates.json.
 * @param notifier Notifier object to catch completion signal. It will be disposed by deleteLater
 * @return Future object to be checked for completion.
 */
std::future<UpdateContents> checkLatestUpdate(
    const QString& updateUrl,
    UpdateCheckNotifier* notifier = nullptr);

/**
 * Checks update for specific build from the internet.
 * It starts asynchronously. If signaler is specified, it will emit notifier->finished.
 * @param updateUrl Url to update server. It should lead directly to updates.json file,
 *     like http://vms_updates:8080/updates/updates.json.
 * @param build Build number, like "28057".
 * @param notifier Notifier object to catch completion signal. It will be disposed by deleteLater
 * @return Future object to be checked for completion.
 */
std::future<UpdateContents> checkSpecificChangeset(
    const QString& updateUrl,
    const QString& build,
    UpdateCheckNotifier* notifier = nullptr);

} // namespace nx::update
