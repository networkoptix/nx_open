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
 * @return first package with specified data, or nullptr if no package is found
 */
const nx::update::Package* findPackage(QString component,
    nx::vms::api::SystemInformation& systemInfo,
    const nx::update::Information& updateInfo);

/**
 * The UpdateCheckSignal class
 * We use this class when regular polling for std::future<UpdateInformation> is not convenient.
 */
class UpdateCheckSignal: public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

signals:
    void atFinished(const nx::update::UpdateContents& contents);
};

/**
 * Checks update from the internet.
 * It starts asynchronously. If signaller is specified, it will emit signaller->atFinished.
 * signaller will be disposed by deleteLater
 * @param signaller signaller to catch completion signal
 * @return future object to be checked for completion
 */
std::future<UpdateContents> checkLatestUpdate(
    QString updateUrl,
    UpdateCheckSignal* signaller = nullptr);

/**
 * Checks update for specific build from the internet.
 * It starts asynchronously. If signaller is specified, it will emit signaller->atFinished.
 * signaller will be disposed by deleteLater
 * @param signaller signaller to catch completion signal
 * @return future object to be checked for completion
 */
std::future<UpdateContents> checkSpecificChangeset(
    QString updateUrl,
    QString build,
    UpdateCheckSignal* signaller = nullptr);

} // namespace nx::update
