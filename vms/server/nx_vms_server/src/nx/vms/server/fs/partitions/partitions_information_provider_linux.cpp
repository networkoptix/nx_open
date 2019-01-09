#include "partitions_information_provider_linux.h"

#include <nx/utils/app_info.h>
#include <nx/vms/server/root_fs.h>

namespace nx::vms::server::fs {

PartitionsInformationProvider::PartitionsInformationProvider(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
    m_fileName = lit("/proc/mounts");
    if (nx::utils::AppInfo::isRaspberryPi())
    {
        // On Rapberry Pi 2+ "/proc/mounts" contains unexisting "/dev/root" on "/", while mount
        // reports a valid mounted device.
        if (system("mount > /root/mounts") == 0)
        {
            // Some devices have /tmp related problems, while root can always access /root.
            m_fileName = lit("/root/mounts");
            m_scanfLongPattern = true;
        }
        else if (system("mount > /tmp/mounts") == 0)
        {
            // Temporary directory /tmp is a fallback for non-root users.
            m_fileName = lit("/tmp/mounts");
            m_scanfLongPattern = true;
        }
    }
}

QByteArray PartitionsInformationProvider::mountsFileContent() const
{
    QFile mountsFile(m_fileName);

    int fd = rootFileSystem()->open(m_fileName, QIODevice::ReadOnly);
    if (fd > 0)
        mountsFile.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle);
    else
        mountsFile.open(QIODevice::ReadOnly);

    if (!mountsFile.isOpen())
        return QByteArray();

    return mountsFile.readAll();
}

bool PartitionsInformationProvider::isScanfLongPattern() const
{
    return m_scanfLongPattern;
}

qint64 PartitionsInformationProvider::totalSpace(const QByteArray& fsPath) const
{
    QnMutexLocker lock(&m_mutex);
    if (m_deviceSpacesCache[fsPath].totalSpace == kUnknownValue)
    {
        m_deviceSpacesCache[fsPath].totalSpace =
            rootFileSystem()->totalSpace(QString::fromLatin1(fsPath));
    }
    return m_deviceSpacesCache[fsPath].totalSpace;
}

qint64 PartitionsInformationProvider::freeSpace(const QByteArray& fsPath) const
{
    QnMutexLocker lock(&m_mutex);
    if (m_deviceSpacesCache[fsPath].freeSpace == kUnknownValue)
    {
        m_deviceSpacesCache[fsPath].freeSpace =
            rootFileSystem()->freeSpace(QString::fromLatin1(fsPath));
    }

    bool freeSpaceIsInvalid = m_deviceSpacesCache[fsPath].freeSpace <= 0;
    if (m_tries++ % 10 == 0 || !freeSpaceIsInvalid)
    {
        m_deviceSpacesCache[fsPath].freeSpace =
            rootFileSystem()->freeSpace(QString::fromLatin1(fsPath));
    }

    // If free space becomes available and total space is invalid let's reset totalSpace to the
    // initial value to let it be checked next iteration.
    if (m_deviceSpacesCache[fsPath].freeSpace > 0 && freeSpaceIsInvalid &&
        m_deviceSpacesCache[fsPath].totalSpace != kUnknownValue &&
        m_deviceSpacesCache[fsPath].totalSpace <= 0)
    {
        m_deviceSpacesCache[fsPath].totalSpace = kUnknownValue;
    }

    return m_deviceSpacesCache[fsPath].freeSpace;
}

bool PartitionsInformationProvider::isFolder(const QByteArray& fsPath) const
{
    auto fileStats = rootFileSystem()->stat(fsPath);
    if (!fileStats.exists)
        return false;

    return fileStats.type == SystemCommands::Stats::FileType::directory;
}

} // namesapce nx::vms::server::fs
