#ifndef UPDATES_COMMON_H
#define UPDATES_COMMON_H

#include <utils/common/software_version.h>

enum class QnCheckForUpdateResult {
    UpdateFound,
    InternetProblem,
    NoNewerVersion,
    NoSuchBuild,
    ServerUpdateImpossible,
    ClientUpdateImpossible,
    BadUpdateFile
};

enum class QnUpdateResult {
    Successful,
    Cancelled,
    LockFailed,
    DownloadingFailed,
    UploadingFailed,
    ClientInstallationFailed,
    InstallationFailed,
    RestInstallationFailed
};

enum class QnPeerUpdateStage {
    Download,           /**< Download update package for the peer. */
    Push,               /**< Push update package to the peer. */
    Install,            /**< Install update. */

    Count
};

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

#endif // UPDATES_COMMON_H
