#pragma once

#include <QtCore/QCoreApplication>

#include <core/resource/resource_fwd.h>
#include <utils/common/software_version.h>
#include <utils/common/system_information.h>
#include <nx/utils/uuid.h>
#include <nx/utils/url.h>

struct QnCheckForUpdateResult
{
    enum Value
    {
        UpdateFound,
        InternetProblem,
        NoNewerVersion,
        NoSuchBuild,
        IncompatibleCloudHost,
        ServerUpdateImpossible,
        ClientUpdateImpossible,
        BadUpdateFile,
        NoFreeSpace,
        DowngradeIsProhibited
    };

    QnCheckForUpdateResult():
        result(NoNewerVersion) {}

    explicit QnCheckForUpdateResult(Value result):
        result(result) {}

    Value result;
    QSet<QnSystemInformation> systems; /**< Set of supported system, for which updates were found. */
    QnSoftwareVersion version;
    bool clientInstallerRequired = false;
    nx::utils::Url releaseNotesUrl;
    QString description;
    QString cloudHost;
};
Q_DECLARE_METATYPE(QnCheckForUpdateResult)

struct QnUpdateResult
{
    Q_DECLARE_TR_FUNCTIONS(QnUpdateResult)
public:

    enum Value
    {
        Successful,
        Cancelled,
        CancelledSilently,
        LockFailed,
        AlreadyUpdated,
        DownloadingFailed,
        DownloadingFailed_NoFreeSpace,
        UploadingFailed,
        UploadingFailed_NoFreeSpace,
        UploadingFailed_Timeout,
        UploadingFailed_Offline,
        UploadingFailed_AuthenticationFailed,
        ClientInstallationFailed,
        InstallationFailed,
        RestInstallationFailed
    };

    QnUpdateResult():
        result(Cancelled)
    {}

    explicit QnUpdateResult(Value result):
        result(result)
    {}

    QString errorMessage() const;

    Value result;
    QnSoftwareVersion targetVersion;
    bool clientInstallerRequired;
    bool protocolChanged;
    QnMediaServerResourceList failedServers;
};
Q_DECLARE_METATYPE(QnUpdateResult)

enum class QnPeerUpdateStage
{
    Init,               /**< Prepare peer update. */
    Download,           /**< Download update package for the peer. */
    Push,               /**< Push update package to the peer. */
    Install,            /**< Install update. */

    Count
};
Q_DECLARE_METATYPE(QnPeerUpdateStage)

enum class QnFullUpdateStage
{
    Init,               /**< Prepare updater tool. */
    Check,              /**< Check for updates. */
    Validate,           /**< Check if update is allowed for the system posiible. */
    Download,           /**< Download update packages. */
    Client,             /**< Install client update. */
    CheckFreeSpace,     /**< Check free space for update files on servers. */
    Push,               /**< Push update packages. */
    Incompatible,       /**< Install updates to the incompatible servers. */
    Servers,            /**< Install updates to the normal servers. */

    Count
};
Q_DECLARE_METATYPE(QnFullUpdateStage)

struct QnUpdateFileInformation
{
    QnSoftwareVersion version;
    QString fileName;
    qint64 fileSize = 0;
    QString baseFileName;
    nx::utils::Url url;
    QString md5;

    QnUpdateFileInformation() {}

    QnUpdateFileInformation(const QnSoftwareVersion &version, const QString &fileName) :
        version(version), fileName(fileName)
    {}

    QnUpdateFileInformation(const QnSoftwareVersion &version, const nx::utils::Url &url) :
        version(version), url(url)
    {}
};
typedef QSharedPointer<QnUpdateFileInformation> QnUpdateFileInformationPtr;

struct QnUpdateTarget
{
    QnUpdateTarget(
        QSet<QnUuid> targets,
        const QnSoftwareVersion& version,
        bool denyClientUpdates = false)
        :
        targets(targets),
        version(version),
        denyClientUpdates(denyClientUpdates)
    {}

    QnUpdateTarget(
        QSet<QnUuid> targets,
        const QString &fileName,
        bool denyClientUpdates = false)
        :
        targets(targets),
        fileName(fileName),
        denyClientUpdates(denyClientUpdates)
    {}

    QSet<QnUuid> targets;
    QnSoftwareVersion version;
    QString fileName;
    bool denyClientUpdates;
};

class QnUpdateUtils
{
public:
    static QSet<QnUuid> getServersLinkedToCloud(const QSet<QnUuid>& peers);
};
