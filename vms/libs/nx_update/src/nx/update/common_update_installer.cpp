#include "common_update_installer.h"

#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/software_version.h>
#include <nx/fusion/model_functions.h>
#include <nx/update/update_information.h>
#include <utils/common/process.h>
#include <utils/common/app_info.h>
#include <utils/common/util.h>
#include <audit/audit_manager.h>
#include <api/model/audit/auth_session.h>
#include <common/common_module.h>

namespace nx {

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

    return QJson::deserialized<update::PackageInformation>(updateInfoFile.readAll());
}

} // namespace

CommonUpdateInstaller::CommonUpdateInstaller(QObject* parent):
    QnCommonModuleAware(parent)
{
}

CommonUpdateInstaller::~CommonUpdateInstaller()
{
}

void CommonUpdateInstaller::prepareAsync(const QString& path)
{
    {
        QnMutexLocker lock(&m_mutex);
        if (m_state == CommonUpdateInstaller::State::inProgress)
            return;

        m_state = CommonUpdateInstaller::State::inProgress;
        if (!cleanInstallerDirectory())
            return setState(CommonUpdateInstaller::State::cleanTemporaryFilesError);
    }

    m_extractor.extractAsync(
        path,
        workDir(),
        [this](QnZipExtractor::Error errorCode, const QString& outputPath)
        {
            auto cleanupGuard = nx::utils::makeScopeGuard(
                [this, errorCode]()
                {
                    if (errorCode != QnZipExtractor::Error::Ok)
                        cleanInstallerDirectory();

                    QnMutexLocker lock(&m_mutex);
                    m_condition.wakeOne();
                });

            NX_DEBUG(
                this,
                lm("ZipExtractor for a path (%1) finished with the code %2")
                    .args(QnZipExtractor::errorToString(errorCode)));

            switch (errorCode)
            {
                case QnZipExtractor::Error::Ok:
                    return setStateLocked(checkContents(outputPath));
                case QnZipExtractor::Error::NoFreeSpace:
                    return setStateLocked(CommonUpdateInstaller::State::noFreeSpace);
                case QnZipExtractor::Error::BrokenZip:
                    return setStateLocked(CommonUpdateInstaller::State::brokenZip);
                case QnZipExtractor::Error::WrongDir:
                    return setStateLocked(CommonUpdateInstaller::State::wrongDir);
                case QnZipExtractor::Error::CantOpenFile:
                    return setStateLocked(CommonUpdateInstaller::State::cantOpenFile);
                case QnZipExtractor::Error::OtherError:
                    return setStateLocked(CommonUpdateInstaller::State::otherError);
                case QnZipExtractor::Error::Stopped:
                    return setStateLocked(CommonUpdateInstaller::State::stopped);
                case QnZipExtractor::Error::Busy:
                    return setStateLocked(CommonUpdateInstaller::State::busy);
            }
        });
}

void CommonUpdateInstaller::setState(CommonUpdateInstaller::State result)
{
    m_state = result;
}

void CommonUpdateInstaller::setStateLocked(CommonUpdateInstaller::State result)
{
    QnMutexLocker lock(&m_mutex);
    m_state = result;
}

CommonUpdateInstaller::State CommonUpdateInstaller::state() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state;
}

CommonUpdateInstaller::State CommonUpdateInstaller::checkContents(const QString& outputPath) const
{
    const update::PackageInformation& info = updateInformation(outputPath);
    if (!info.isValid())
        return CommonUpdateInstaller::State::updateContentsError;

    if (info.component != component())
    {
        NX_ERROR(this, "Update package is for %1, not %2", info.component, component());
        return CommonUpdateInstaller::State::updateContentsError;
    }

    const auto& osInfo = utils::OsInfo::current();
    if (!info.isCompatibleTo(osInfo))
    {
        NX_ERROR(this, "Update is incompatible to current %1:\n%2",
            osInfo, QString::fromUtf8(QJson::serialized(info)));
        return CommonUpdateInstaller::State::updateContentsError;
    }

    if (info.installScript.isEmpty())
    {
        NX_ERROR(this, "No executable specified in the update information file");
        return CommonUpdateInstaller::State::updateContentsError;
    }

    m_executable = QDir(outputPath).absoluteFilePath(info.installScript);
    if (!checkExecutable(m_executable))
    {
        NX_ERROR(this, "Update executable file is invalid");
        return CommonUpdateInstaller::State::updateContentsError;
    }

    m_version = info.version;

    return CommonUpdateInstaller::State::ok;
}

bool CommonUpdateInstaller::install(const QnAuthSession& authInfo)
{
    QString installerDir = workDir();

    QStringList arguments;
    QString logFileName;

    if (initializeUpdateLog(m_version, &logFileName))
        arguments.append(logFileName);
    else
        NX_WARNING(this, "Failed to create or open update log file.");

    auto authRecord = commonModule()->auditManager()->prepareRecord(authInfo, Qn::AR_UpdateInstall);
    authRecord.addParam("version", m_version.toLatin1());
    commonModule()->auditManager()->addAuditRecord(authRecord);

    QString installerPath = QDir(workDir()).absoluteFilePath(m_executable);
    SystemError::ErrorCode error = nx::startProcessDetached(installerPath, arguments, installerDir);
    if (error == SystemError::noError)
    {
        auto argumentsString = lm("\"%1\"").arg(arguments.join("\" \""));
        NX_INFO(this, lm("Update has been started. file=\"%1\", args=%2").args(installerPath, argumentsString));
    }
    else
    {
        NX_ERROR(this, lm("Failed to launch update script. %1").args(SystemError::toString(error)));
    }

    return true;
}

QString CommonUpdateInstaller::workDir() const
{
    QString selfPath = commonModule()->moduleGUID().toString();
    // This path will look like /tmp/nx_isntaller-server_guid/
    // We add server_guid to allow to run multiple servers on a single machine.
    return closeDirPath(dataDirectoryPath()) + "nx_installer-" + selfPath;
}

void CommonUpdateInstaller::stopSync(bool clearAndReset)
{
    QnMutexLocker lock(&m_mutex);
    while (m_state == CommonUpdateInstaller::State::inProgress)
        m_condition.wait(lock.mutex());
    if (clearAndReset)
    {
        m_state = CommonUpdateInstaller::State::idle;
        cleanInstallerDirectory();
    }
}

bool CommonUpdateInstaller::cleanInstallerDirectory()
{
    QString path = workDir();
    const auto removeResult = QDir(path).removeRecursively();
    const auto fileInfo = QFileInfo(path);
    NX_DEBUG(
        this,
        "Cleaning up the installer's temporary directory (%1). Exists: %2, owner: %3, group: %4, remove succeded: %5",
        path, fileInfo.exists(), fileInfo.owner(), fileInfo.group(), removeResult);
    if (!removeResult)
        return false;
    return QDir().mkpath(path);
}

bool CommonUpdateInstaller::checkExecutable(const QString& executableName) const
{
    QFile executableFile(QDir(workDir()).absoluteFilePath(executableName));
    if (!executableFile.exists())
    {
        NX_ERROR(
            this,
            lm("The specified executable doesn't exists: %1").args(executableName));
        return false;
    }

    if (!executableFile.permissions().testFlag(QFile::ExeOwner)
        && !executableFile.setPermissions(executableFile.permissions() | QFile::ExeOwner))
    {
        NX_ERROR(
            this,
            lm("Failed to set file permissions: %1").args(executableName));
        return false;
    }

    return true;
}

} // namespace nx
