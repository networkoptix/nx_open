#include "update_installer.h"

#include <chrono>

#include <QtCore/QCoreApplication>

#include <nx/utils/log/log.h>
#include <nx/utils/log/log_message.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/app_info.h>
#include <nx/update/update_information.h>
#include <utils/common/process.h>
#include <nx/update/update_check.h>
#include <audit/audit_manager.h>
#include <api/model/audit/auth_session.h>
#include <common/common_module.h>
#include <media_server/media_server_module.h>
#include <media_server/settings.h>
#include <nx/vms/common/utils/file_signature.h>
#include <nx/vms/server/root_fs.h>
#include <nx_vms_server_ini.h>

using namespace std::chrono;

namespace nx::vms::server {

namespace {

update::PackageInformation updateInformation(const QString& path)
{
    const QString infoFileName = QDir(path).absoluteFilePath("package.json");
    QFile updateInfoFile(infoFileName);
    updateInfoFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!updateInfoFile.open(QFile::ReadOnly))
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to open update information file: %1", infoFileName);
        return {};
    }

    bool ok = false;
    const auto& result = QJson::deserialized<update::PackageInformation>(
        updateInfoFile.readAll(), {}, &ok);

    if (!ok || !result.isValid())
        NX_ERROR(NX_SCOPE_TAG, "Cannot deserialize info from %1", infoFileName);

    return result;
}

common::FileSignature::Result verifyFile(
    const QString& fileName, const QByteArray& signature, const QString& additionalKeysDir)
{
    if (ini().skipUpdateFilesVerification)
        return common::FileSignature::Result::ok;

    const QString kCertificatesDir = ":/update_verification_keys";

    QList<QByteArray> keys;

    const auto loadKeys =
        [&keys](const QDir& dir)
        {
            for (const QString& file: dir.entryList({"*.pem"}, QDir::Files, QDir::Name))
            {
                const QString& fileName = dir.absoluteFilePath(file);
                const QByteArray& key = common::FileSignature::readKey(fileName);
                if (key.isEmpty())
                    NX_WARNING(typeid(UpdateInstaller), "Cannot load key from %1", fileName);
                keys.append(key);
            }
        };

    if (!additionalKeysDir.isEmpty())
        loadKeys(additionalKeysDir);

    loadKeys(kCertificatesDir);

    for (const auto& key: keys)
    {
        const auto verificationResult = common::FileSignature::verify(fileName, key, signature);
        if (verificationResult == common::FileSignature::Result::ok)
            return verificationResult;
    }

    return common::FileSignature::Result::failed;
}

} // namespace

static QString toString(UpdateInstaller::State state)
{
    using State = UpdateInstaller::State;

    switch (state)
    {
        case State::idle: return "idle";
        case State::ok: return "ok";
        case State::inProgress: return "inProgress";
        case State::noFreeSpace: return "noFreeSpace";
        case State::noFreeSpaceToInstall: return "noFreeSpaceToInstall";
        case State::brokenZip: return "brokenZip";
        case State::verificationFailed: return "verificationFailed";
        case State::wrongDir: return "wrongDir";
        case State::cantOpenFile: return "cantOpenFile";
        case State::otherError: return "otherError";
        case State::stopped: return "stopped";
        case State::busy: return "busy";
        case State::installing: return "installing";
        case State::installationFailed: return "installationFailed";
        case State::cleanTemporaryFilesError: return "cleanTemporaryFilesError";
        case State::updateContentsError: return "updateContentsError";
        case State::unknownError: return "unknownError";
    }

    return QString();
}

UpdateInstaller::UpdateInstaller(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    m_installationTimer(new QTimer()),
    m_installationCheckTimer(new QTimer())
{
    m_installationTimer->setInterval(ini().autoUpdateInstallationDelayMs);
    QObject::connect(m_installationTimer.get(), &QTimer::timeout,
        [this]() { install(QnAuthSession()); });

    m_installationCheckTimer->setInterval(30s);
    QObject::connect(m_installationCheckTimer.get(), &QTimer::timeout,
        [this]()
        {
            // Installation process must restart the server. If it didn't happen and the installer
            // process is finished, something went wrong.
            if (!checkProcessExists(m_installationProcessPid))
            {
                NX_ERROR(this, "Failed to install update. "
                    "Installation process exited, but server is still running.");
                setStateLocked(State::installationFailed);
                m_installationCheckTimer->stop();
            }
        });
}

UpdateInstaller::~UpdateInstaller()
{
}

void UpdateInstaller::prepareAsync(const QString& path, const QByteArray& signature)
{
    if (!serverModule()->rootFileSystem()->changeOwner(workDir()))
        NX_WARNING(this, "Unable to chown %1", workDir());

    NX_DEBUG(this, "Preparing to update installation...");

    const auto verificationResult = verifyFile(
        path, signature, serverModule()->settings().additionalUpdateVerificationKeysDir());
    NX_DEBUG(this, "Update file verification result for %1: %2", path, verificationResult);

    switch (verificationResult)
    {
        case nx::vms::common::FileSignature::Result::ok:
            break;

        case nx::vms::common::FileSignature::Result::failed:
            NX_ERROR(this, "Update file verification failed: %1", path);
            setState(State::verificationFailed);
            return;

        case nx::vms::common::FileSignature::Result::ioError:
            setState(State::cantOpenFile);
            return;

        default:
            setState(State::otherError);
            return;
    }

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        if (m_state == State::inProgress)
            return;

        m_state = State::inProgress;
        if (!cleanInstallerDirectory())
            return setState(State::cleanTemporaryFilesError);
    }

    {
        qint64 requiredSpace = QnZipExtractor(path, QString()).estimateUnpackedSize();
        if (requiredSpace < -1)
            return setState(State::cantOpenFile);

        if (!checkFreeSpace(dataDirectoryPath(), requiredSpace + update::reservedSpacePadding()))
            return setState(State::noFreeSpace);
    }

    const QString outputDir = workDir();
    NX_DEBUG(this, "Start update file extraction: %1 to %2", path, outputDir);
    m_extractor.extractAsync(path, outputDir,
        [this](QnZipExtractor::Error errorCode, const QString& outputPath, qint64 bytesExtracted)
        {
            auto cleanupGuard = nx::utils::makeScopeGuard(
                [this, errorCode]()
                {
                    if (errorCode != QnZipExtractor::Error::Ok)
                        cleanInstallerDirectory();

                    NX_MUTEX_LOCKER lock(&m_mutex);
                    m_condition.wakeOne();
                });

            NX_DEBUG(this, "ZipExtractor for a path (%1) finished with the code %2",
                outputPath, QnZipExtractor::errorToString(errorCode));

            m_bytesExtracted = bytesExtracted;

            switch (errorCode)
            {
                case QnZipExtractor::Error::Ok:
                    return setStateLocked(checkContents(outputPath));
                case QnZipExtractor::Error::NoFreeSpace:
                    return setStateLocked(State::noFreeSpace);
                case QnZipExtractor::Error::BrokenZip:
                    return setStateLocked(State::brokenZip);
                case QnZipExtractor::Error::WrongDir:
                    return setStateLocked(State::wrongDir);
                case QnZipExtractor::Error::CantOpenFile:
                    return setStateLocked(State::cantOpenFile);
                case QnZipExtractor::Error::OtherError:
                    return setStateLocked(State::otherError);
                case QnZipExtractor::Error::Stopped:
                    return setStateLocked(State::stopped);
                case QnZipExtractor::Error::Busy:
                    return setStateLocked(State::busy);
            }
    });
}

void UpdateInstaller::install(const QnAuthSession& authInfo)
{
    stopInstallationTimerAsync();

    if (const auto currentState = state(); currentState != UpdateInstaller::State::ok)
    {
        NX_ERROR(this, "Ignoring installation request. Current state: %1.", (int) currentState);
        return;
    }

    NX_INFO(this, "Starting installation...");

    QString installerDir = workDir();

    QStringList arguments;
    QString logFileName;

    if (initializeUpdateLog(m_version, &logFileName))
        arguments.append(logFileName);
    else
        NX_WARNING(this, "Failed to create or open update log file.");

    if (!authInfo.id.isNull())
    {
        auto authRecord = auditManager()->prepareRecord(authInfo, Qn::AR_UpdateInstall);
        authRecord.addParam("version", m_version.toLatin1());
        auditManager()->addAuditRecord(authRecord);
    }

    QString installerPath = QDir(workDir()).absoluteFilePath(m_executable);
    SystemError::ErrorCode error =
        startProcessDetached(installerPath, arguments, installerDir, &m_installationProcessPid);

    if (error != SystemError::noError)
    {
        NX_ERROR(this, "Failed to launch the update script. %1", SystemError::toString(error));
        return;
    }

    setStateLocked(State::installing);

    NX_INFO(this, "Update has been started. file=\"%1\", args=[%2]",
        installerPath, arguments.join(", "));

    QTimer::singleShot(0, m_installationCheckTimer.get(), qOverload<>(&QTimer::start));
}

void UpdateInstaller::installDelayed()
{
    if (m_installationTimer->isActive())
        return;

    if (state() != State::ok)
        return;

    NX_INFO(this, "Delayed installation requested. Starting in %1ms",
        m_installationTimer->interval());

    QTimer::singleShot(0, m_installationTimer.get(), qOverload<>(&QTimer::start));
}

void UpdateInstaller::stopSync(bool clearAndReset)
{
    NX_DEBUG(this, "Stopping...");

    stopInstallationTimerAsync();

    NX_MUTEX_LOCKER lock(&m_mutex);
    while (m_state == State::inProgress)
        m_condition.wait(lock.mutex());
    if (clearAndReset)
    {
        m_state = State::idle;
        cleanInstallerDirectory();
    }
}

void UpdateInstaller::recheckFreeSpaceForInstallation()
{
    const auto state = this->state();
    if (state != State::noFreeSpaceToInstall && state != State::ok)
        return;

    setStateLocked(checkFreeSpaceForInstallation() ? State:: ok : State::noFreeSpaceToInstall);
}

UpdateInstaller::State UpdateInstaller::state() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_state;
}

bool UpdateInstaller::checkFreeSpace(const QString& path, qint64 bytes) const
{
    const qint64 free = freeSpace(path);

    const auto requiredMb = bytes / (1024 * 1024);
    const auto freeMb = free / (1024 * 1024);

    NX_DEBUG(this, "Checking free space in %1. Required: %2MB, Free: %3MB",
        path, requiredMb, freeMb);

    if (free < bytes)
    {
        NX_WARNING(this, "Not enough free space in %1. Required: %2MB, Free: %3MB",
            path, requiredMb, freeMb);
        return false;
    }

    return true;
}

bool UpdateInstaller::checkFreeSpaceForInstallation() const
{
    const qint64 required = m_freeSpaceRequiredToUpdate > 0
        ? m_freeSpaceRequiredToUpdate
        // If we don't know exactly how much space is required, assume we need 10% of extracted
        // size after extraction over old files.
        : m_bytesExtracted / 10;

    return checkFreeSpace(
        QCoreApplication::applicationDirPath(), required + update::reservedSpacePadding());
}

QString UpdateInstaller::dataDirectoryPath() const
{
    return QDir::temp().absolutePath();
}

QString UpdateInstaller::component() const
{
    return QStringLiteral("server");
}

int64_t UpdateInstaller::freeSpace(const QString& path) const
{
    return serverModule()->rootFileSystem()->freeSpace(path);
}

bool UpdateInstaller::initializeUpdateLog(
    const QString& targetVersion, QString* logFileName) const
{
    QString logDir;
    if (settings().logDir.present())
        logDir = settings().logDir();
    else
        logDir = settings().dataDir() + "/log/";

    if (logDir.isEmpty())
    {
        NX_WARNING(this, "Failed to find a proper path for a log file for version %1 installer",
            targetVersion);
        return false;
    }

    QString fileName = QDir(logDir).absoluteFilePath("update.log");
    QFile logFile(fileName);
    if (!logFile.open(QFile::Append))
    {
        NX_WARNING(this, "Failed to create log file %1 for version %2 installer",
            fileName, targetVersion);
        return false;
    }

    const nx::utils::SoftwareVersion& currentVersion =
        serverModule()->commonModule()->engineVersion();

    QByteArray preface;
    preface.append("====================\n");
    preface.append(nx::format(" [%1] Starting system update:\n", QDateTime::currentDateTime()));
    preface.append(nx::format("    Current version: %1\n", currentVersion));
    preface.append(nx::format("    Target version: %1\n", targetVersion));
    preface.append("--------------------\n");

    logFile.write(preface);
    logFile.close();

    *logFileName = fileName;
    NX_INFO(this, "Initialized log file %1 for version %2 installer", fileName, targetVersion);
    return true;
}

void UpdateInstaller::setState(UpdateInstaller::State state)
{
    if (m_state == state)
        return;

    m_state = state;
    NX_DEBUG(this, "State changed: %1", state);
}

void UpdateInstaller::setStateLocked(UpdateInstaller::State state)
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        setState(state);
    }

    if (state != State::ok)
        stopInstallationTimerAsync();
}

bool UpdateInstaller::cleanInstallerDirectory()
{
    const QString path = workDir();
    const auto removeResult = QDir(path).removeRecursively();
    const auto fileInfo = QFileInfo(path);
    NX_DEBUG(this,
        "Cleaning up the installer's temporary directory (%1). "
            "Exists: %2, owner: %3, group: %4, remove succeded: %5",
        path, fileInfo.exists(), fileInfo.owner(), fileInfo.group(), removeResult);
    if (!removeResult)
        return false;
    return QDir().mkpath(path);
}

bool UpdateInstaller::checkExecutable(const QString& executableName) const
{
    QFile executableFile(QDir(workDir()).absoluteFilePath(executableName));
    if (!executableFile.exists())
    {
        NX_ERROR(this, "The specified executable doesn't exists: %1", executableName);
        return false;
    }

    if (!executableFile.permissions().testFlag(QFile::ExeOwner)
        && !executableFile.setPermissions(executableFile.permissions() | QFile::ExeOwner))
    {
        NX_ERROR(this, "Failed to set file permissions: %1", executableName);
        return false;
    }

    return true;
}

UpdateInstaller::State UpdateInstaller::checkContents(const QString& outputPath) const
{
    const update::PackageInformation& info = updateInformation(outputPath);
    if (!info.isValid())
        return State::updateContentsError;

    if (info.component != component())
    {
        NX_ERROR(this, "Update package is for %1, not %2", info.component, component());
        return State::updateContentsError;
    }

    const auto& osInfo = utils::OsInfo::current();
    if (!info.isCompatibleTo(osInfo))
    {
        NX_ERROR(this, "Update is incompatible to current %1:\n%2",
            osInfo, QString::fromUtf8(QJson::serialized(info)));
        return State::updateContentsError;
    }

    if (info.installScript.isEmpty())
    {
        NX_ERROR(this, "No executable specified in the update information file");
        return State::updateContentsError;
    }

    m_executable = QDir(outputPath).absoluteFilePath(info.installScript);
    if (!checkExecutable(m_executable))
    {
        NX_ERROR(this, "Update executable file is invalid");
        return State::updateContentsError;
    }

    m_version = info.version;
    m_freeSpaceRequiredToUpdate = info.freeSpaceRequired;

    if (!checkFreeSpaceForInstallation())
    {
        NX_ERROR(this, "Not enough free space for installation");
        return State::noFreeSpaceToInstall;
    }

    return State::ok;
}

QString UpdateInstaller::workDir() const
{
    const auto& selfPath = moduleGUID().toString();
    // This path will look like /tmp/networkoptix_installer-{server_guid}/
    // We add server_guid to allow running multiple servers on a single machine.
    return closeDirPath(dataDirectoryPath())
        + nx::utils::AppInfo::linuxOrganizationName() + "_installer-" + selfPath;
}

void UpdateInstaller::stopInstallationTimerAsync()
{
    QTimer::singleShot(0, m_installationTimer.get(), &QTimer::stop);
}

} // namespace nxvms::server
