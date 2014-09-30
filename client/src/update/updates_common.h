#ifndef UPDATES_COMMON_H
#define UPDATES_COMMON_H

#include <utils/common/software_version.h>
#include <utils/common/system_information.h>
#include <utils/common/uuid.h>


struct QnCheckForUpdateResult {
    enum Value {
        UpdateFound,
        InternetProblem,
        NoNewerVersion,
        NoSuchBuild,
        ServerUpdateImpossible,
        ClientUpdateImpossible,
        BadUpdateFile
    };

    QnCheckForUpdateResult():
        result(NoNewerVersion) {}

    explicit QnCheckForUpdateResult(Value result):
        result(result) {}

    Value result;
    QSet<QnSystemInformation> systems;  /**< Set of supported system, for which updates were found. */
    QnSoftwareVersion latestVersion;
    bool clientInstallerRequired;
    QUrl releaseNotesUrl;
};
Q_DECLARE_METATYPE(QnCheckForUpdateResult);


struct QnUpdateResult {
    enum Value {
        Successful,
        Cancelled,
        LockFailed,
        DownloadingFailed,
        DownloadingFailed_NoFreeSpace,
        UploadingFailed,
        UploadingFailed_NoFreeSpace,
        ClientInstallationFailed,
        InstallationFailed,
        RestInstallationFailed
    };

    QnUpdateResult():
        result(Cancelled) {}

    explicit QnUpdateResult(Value result):
        result(result) {}

    Value result;
    QnSoftwareVersion targetVersion;
    bool clientInstallerRequired;
};
Q_DECLARE_METATYPE(QnUpdateResult);

enum class QnPeerUpdateStage {
    Init,               /**< Prepare peer update. */
    Download,           /**< Download update package for the peer. */
    Push,               /**< Push update package to the peer. */
    Install,            /**< Install update. */

    Count
};
Q_DECLARE_METATYPE(QnPeerUpdateStage);

enum class QnFullUpdateStage {
    Init,               /**< Prepare updater tool. */
    Check,              /**< Check for updates. */
    Download,           /**< Download update packages. */
    Client,             /**< Install client update. */
    Incompatible,       /**< Install updates to the incompatible servers. */
    Push,               /**< Push update package to the normal servers. */
    Servers,            /**< Install updates to the normal servers. */

    Count
};
Q_DECLARE_METATYPE(QnFullUpdateStage);

struct QnUpdateFileInformation {
    QnSoftwareVersion version;
    QString fileName;
    qint64 fileSize;
    QString baseFileName;
    QUrl url;
    QString md5;

    QnUpdateFileInformation() {}

    QnUpdateFileInformation(const QnSoftwareVersion &version, const QString &fileName) :
        version(version), fileName(fileName)
    {}

    QnUpdateFileInformation(const QnSoftwareVersion &version, const QUrl &url) :
        version(version), url(url)
    {}
};
typedef QSharedPointer<QnUpdateFileInformation> QnUpdateFileInformationPtr;

struct QnUpdateTarget {

    QnUpdateTarget(QSet<QnUuid> targets, const QnSoftwareVersion &version, bool denyClientUpdates = false, bool denyMajorUpdates = false) :
        targets(targets),
        version(version),
        denyClientUpdates(denyClientUpdates),
        denyMajorUpdates(denyMajorUpdates)
    {}

    QnUpdateTarget(QSet<QnUuid> targets, const QString &fileName, bool denyClientUpdates = false, bool denyMajorUpdates = false) :
        targets(targets),
        fileName(fileName),
        denyClientUpdates(denyClientUpdates),
        denyMajorUpdates(denyMajorUpdates)
    {}

    QSet<QnUuid> targets;
    QnSoftwareVersion version;
    QString fileName;
    bool denyClientUpdates;
    bool denyMajorUpdates;
};

#endif // UPDATES_COMMON_H
