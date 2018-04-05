#include "common_updates2_installer.h"
#include <nx/update/installer/detail/zip_extractor.h>
#include <nx/utils/log/log.h>
#include <utils/common/system_information.h>
#include <QtCore>

namespace nx {
namespace update {

namespace {

const QString kUpdateInfoFileName = "update.json";

} // namespace

bool CommonUpdates2Installer::cleanInstallerDirectory()
{
    return QDir(installerWorkDir()).removeRecursively() && QDir().mkpath(installerWorkDir());
}

installer::detail::AbstractZipExtractorPtr CommonUpdates2Installer::createZipExtractor() const
{
    return std::make_shared<installer::detail::ZipExtractor>();
}

QVariantMap CommonUpdates2Installer::updateInformation(const QString& outputPath) const
{
    QFile updateInfoFile(QDir(outputPath).absoluteFilePath(kUpdateInfoFileName));
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

QnSystemInformation CommonUpdates2Installer::systemInformation() const
{
    return QnSystemInformation::currentSystemInformation();
}

bool CommonUpdates2Installer::checkExecutable(const QString& executableName) const
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

} // namespace updates2
} // namespace nx
