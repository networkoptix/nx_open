#include "client_installation.h"

#include <QtCore/QDir>
#include <QtCore/QFile>

#include <nx/utils/log/log.h>
#include <nx/utils/app_info.h>

#include "applauncher_app_info.h"

namespace {

static const QString kBuildInfoFile = "build_info.txt";

} // namespace

bool QnClientInstallation::exists() const
{
    return !m_binaryPath.isEmpty() && QFile::exists(executableFilePath());
}

nx::utils::SoftwareVersion QnClientInstallation::version() const
{
    return m_version;
}

void QnClientInstallation::setVersion(const nx::utils::SoftwareVersion& version)
{
    m_version = version;
}

int QnClientInstallation::protocolVersion() const
{
    return m_protoVersion;
}

QString QnClientInstallation::rootPath() const
{
    return m_rootPath;
}

QString QnClientInstallation::binaryPath() const
{
    return QFileInfo(executableFilePath()).absolutePath();
}

QString QnClientInstallation::executableFilePath() const
{
    return m_binaryPath.isEmpty() ? QString() : m_rootPath + "/" + m_binaryPath;
}

QString QnClientInstallation::libraryPath() const
{
    return m_libPath.isEmpty() ? QString() : m_rootPath + "/" + m_libPath;
}

QString targetBinaryPath(QString binaryPath)
{
    QString prefix;
    if (nx::utils::AppInfo::isLinux())
        prefix = "bin/";
    else if (nx::utils::AppInfo::isMacOsX())
        prefix = QnApplauncherAppInfo::bundleName() +  "/Contents/MacOS/";

    return binaryPath.prepend(prefix);
}

QnClientInstallationPtr QnClientInstallation::installationForPath(const QString& rootPath)
{
    QDir rootDir(rootPath);

    QnClientInstallationPtr installation(new QnClientInstallation());
    installation->m_rootPath = rootPath;

    const QString binary = targetBinaryPath(QnApplauncherAppInfo::clientBinaryName());
    if (!rootDir.exists(binary))
        return QnClientInstallationPtr();

    installation->m_binaryPath = binary;

    if (rootDir.exists(kBuildInfoFile))
    {
        QFile buildInfo(rootDir.absoluteFilePath(kBuildInfoFile));
        if (buildInfo.open(QIODevice::ReadOnly))
        {
            QRegExp pattern("(.*)=(.*)");

            QStringList details = QString::fromUtf8(buildInfo.readAll())
                .split(L'\n', QString::SkipEmptyParts);
            for (const auto& item: details)
            {
                if (pattern.exactMatch(item.trimmed()))
                {
                    QString key = pattern.cap(1).trimmed();
                    QString value = pattern.cap(2).trimmed();
                    if (key == "protoVersion")
                        installation->m_protoVersion = value.toInt();
                    else if (key == "version")
                        installation->m_version = nx::utils::SoftwareVersion(value);
                }
            }
        }
    }

    QString lib = "lib";
    if (rootDir.exists(lib))
    {
        installation->m_libPath = lib;
    }
    else
    {
        lib.prepend("../");
        if (rootDir.exists(lib))
            installation->m_libPath = lib;
    }

    return installation;
}
