// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_installation.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QSettings>

#include <nx/utils/log/log.h>

#include <nx/branding.h>
#include <nx/build_info.h>

using namespace std::chrono;

namespace {

static const QString kBuildInfoFile = "build_info.txt";

static const QString kPropertiesFile = "properties.ini";
static const QString kLastExecutionDateTimeKey = "stats/lastExecutionDateTime";

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

bool QnClientInstallation::canBeRemoved()
{
    return m_canBeRemoved;
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
    if (nx::build_info::isLinux())
        prefix = "bin/";
    else if (nx::build_info::isMacOsX())
        prefix = "Contents/MacOS/";

    return binaryPath.prepend(prefix);
}

QnClientInstallationPtr QnClientInstallation::installationForPath(const QString& rootPath, bool canBeRemoved)
{
    QDir rootDir(rootPath);

    QnClientInstallationPtr installation(new QnClientInstallation());
    installation->m_rootPath = rootPath;
    installation->m_canBeRemoved = canBeRemoved;

    const QString binary = targetBinaryPath(nx::branding::desktopClientLauncherName());
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
                .split(L'\n', Qt::SkipEmptyParts);
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

    if (rootDir.exists(kPropertiesFile))
    {
        QSettings ini(rootDir.absoluteFilePath(kPropertiesFile), QSettings::IniFormat);
        installation->m_lastExecutionTime =
            milliseconds(ini.value(kLastExecutionDateTimeKey, 0).toLongLong());
    }
    if (installation->m_lastExecutionTime == 0ms)
        installation->updateLastExecutionTime();

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

milliseconds QnClientInstallation::lastExecutionTime() const
{
    return m_lastExecutionTime;
}

void QnClientInstallation::updateLastExecutionTime()
{
    m_lastExecutionTime = duration_cast<milliseconds>(system_clock::now().time_since_epoch());

    QSettings ini(QDir(m_rootPath).absoluteFilePath(kPropertiesFile), QSettings::IniFormat);
    ini.setValue(kLastExecutionDateTimeKey, (qint64) m_lastExecutionTime.count());
}
