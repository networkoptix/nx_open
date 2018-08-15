#include "common_update_installer.h"
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/software_version.h>
#include <utils/common/process.h>
#include <utils/common/app_info.h>
#include <audit/audit_manager.h>

#include <QtCore>

namespace nx {

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
        installerWorkDir(),
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

            switch (errorCode)
            {
                case QnZipExtractor::Error::Ok:
                    return setStateLocked(checkContents(outputPath));
                case QnZipExtractor::Error::NoFreeSpace:
                    return setStateLocked(CommonUpdateInstaller::State::noFreeSpace);
                default:
                    NX_WARNING(
                        this,
                        lm("ZipExtractor error: %1").args(QnZipExtractor::errorToString(errorCode)));
                    return setStateLocked(CommonUpdateInstaller::State::unknownError);
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
    QVariantMap infoMap = updateInformation(outputPath);
    if (infoMap.isEmpty())
        return CommonUpdateInstaller::State::updateContentsError;

    if (infoMap.value("executable").toString().isEmpty())
    {
        NX_ERROR(this, "No executable specified in the update information file");
        return CommonUpdateInstaller::State::updateContentsError;
    }

    m_executable = outputPath + QDir::separator() + infoMap.value("executable").toString();
    if (!checkExecutable(m_executable))
    {
        NX_ERROR(this, "Update executable file is invalid");
        return CommonUpdateInstaller::State::updateContentsError;
    }

    const auto systemInfo = systemInformation();

    QString platform = infoMap.value("platform").toString();
    if (platform != systemInfo.platform)
    {
        NX_ERROR(this, lm("Incompatible update: %1 != %2").args(systemInfo.platform, platform));
        return CommonUpdateInstaller::State::updateContentsError;
    }

    QString arch = infoMap.value("arch").toString();
    if (arch != systemInfo.arch)
    {
        NX_ERROR(this, lm("Incompatible update: %1 != %2").args(systemInfo.arch, arch));
        return CommonUpdateInstaller::State::updateContentsError;
    }

    QString modification = infoMap.value("modification").toString();
    if (modification != systemInfo.modification)
    {
        NX_ERROR(this, lm("Incompatible update: %1 != %2").args(systemInfo.modification, modification));
        return CommonUpdateInstaller::State::updateContentsError;
    }

    QString variantVersion = infoMap.value("variantVersion").toString();
    if (nx::utils::SoftwareVersion(variantVersion) > nx::utils::SoftwareVersion(systemInfo.version))
    {
        NX_ERROR(this, lm("Incompatible update: %1 > %2").args(variantVersion, systemInfo.version));
        return CommonUpdateInstaller::State::updateContentsError;
    }

    m_version = infoMap.value(QStringLiteral("version")).toString();

    return CommonUpdateInstaller::State::ok;
}

bool CommonUpdateInstaller::install(const QnAuthSession& authInfo)
{
    QString currentDir = QDir::currentPath();
    QDir::setCurrent(installerWorkDir());

    QStringList arguments;
    QString logFileName;

    if (initializeUpdateLog(m_version, &logFileName))
        arguments.append(logFileName);
    else
        NX_WARNING(this, "Failed to create or open update log file.");

    if (nx::utils::log::mainLogger()->isToBeLogged(nx::utils::log::Level::debug))
    {
        QString argumentsStr(
            " APPSERVER_PASSWORD=\"\" APPSERVER_PASSWORD_CONFIRM=\"\" SERVER_PASSWORD=\"\"\
             SERVER_PASSWORD_CONFIRM=\"\"");
        for (const QString& arg: arguments)
            argumentsStr += " " + arg;

        NX_INFO(this, lm("Launching %1 %2").arg(m_executable).args(argumentsStr));
    }

    auto authRecord = qnAuditManager->prepareRecord(authInfo, Qn::AR_UpdateInstall);
    authRecord.addParam("version", m_version.toLatin1());
    qnAuditManager->addAuditRecord(authRecord);

    const SystemError::ErrorCode processStartErrorCode = nx::startProcessDetached(
        QDir(installerWorkDir()).absoluteFilePath(m_executable),
        arguments);
    if (processStartErrorCode == SystemError::noError)
    {
        NX_INFO(this, "Update has been started.");
    }
    else
    {
        NX_ERROR(
            this,
            lm("Failed to launch update script. %1")
                .args(SystemError::toString(processStartErrorCode)));
    }

    QDir::setCurrent(currentDir);

    return true;
}

QString CommonUpdateInstaller::installerWorkDir() const
{
    return dataDirectoryPath() + QDir::separator() + ".installer";
}

void CommonUpdateInstaller::stopSync()
{
    QnMutexLocker lock(&m_mutex);
    while (m_state == CommonUpdateInstaller::State::inProgress)
        m_condition.wait(lock.mutex());
    m_state = CommonUpdateInstaller::State::idle;
}

CommonUpdateInstaller::~CommonUpdateInstaller()
{
    stopSync();
}

bool CommonUpdateInstaller::cleanInstallerDirectory()
{
    return QDir(installerWorkDir()).removeRecursively() && QDir().mkpath(installerWorkDir());
}

QVariantMap CommonUpdateInstaller::updateInformation(const QString& outputPath) const
{
    QFile updateInfoFile(QDir(outputPath).absoluteFilePath("update.json"));
    updateInfoFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!updateInfoFile.open(QFile::ReadOnly))
    {
        NX_ERROR(
            this,
            lm("Failed to open update information file: %1").args(updateInfoFile.fileName()));
        return QVariantMap();
    }

    return QJsonDocument::fromJson(updateInfoFile.readAll()).toVariant().toMap();
}

nx::vms::api::SystemInformation CommonUpdateInstaller::systemInformation() const
{
    return QnAppInfo::currentSystemInformation();
}

bool CommonUpdateInstaller::checkExecutable(const QString& executableName) const
{
    QFile executableFile(QDir(installerWorkDir()).absoluteFilePath(executableName));
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
