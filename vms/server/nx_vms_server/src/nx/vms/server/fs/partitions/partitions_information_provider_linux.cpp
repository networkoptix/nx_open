#include "partitions_information_provider_linux.h"

#include <nx/utils/app_info.h>
#include <nx/vms/server/root_fs.h>
#include <api/global_settings.h>

namespace nx::vms::server::fs {

PartitionsInformationProvider::PartitionsInformationProvider(QnGlobalSettings *globalSettings, RootFileSystem *rootFs):
    m_globalSettings(globalSettings),
    m_rootFs(rootFs),
    m_fileName("/proc/mounts")
{
}

QByteArray PartitionsInformationProvider::mountsFileContents() const
{
    QFile mountsFile(m_fileName);

    int fd = m_rootFs->open(m_fileName, QIODevice::ReadOnly);
    if (fd > 0)
        mountsFile.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle);
    else
        mountsFile.open(QIODevice::ReadOnly);

    if (!mountsFile.isOpen())
        return QByteArray();

    return mountsFile.readAll();
}

qint64 PartitionsInformationProvider::totalSpace(const QByteArray& fsPath) const
{
    QnMutexLocker lock(&m_mutex);
    if (m_deviceSpacesCache[fsPath].totalSpace == kUnknownValue)
    {
        m_deviceSpacesCache[fsPath].totalSpace =
            m_rootFs->totalSpace(QString::fromLatin1(fsPath));
    }
    return m_deviceSpacesCache[fsPath].totalSpace;
}

qint64 PartitionsInformationProvider::freeSpace(const QByteArray& fsPath) const
{
    QnMutexLocker lock(&m_mutex);
    if (m_deviceSpacesCache[fsPath].freeSpace == kUnknownValue)
    {
        m_deviceSpacesCache[fsPath].freeSpace =
            m_rootFs->freeSpace(QString::fromLatin1(fsPath));
    }

    bool freeSpaceIsInvalid = m_deviceSpacesCache[fsPath].freeSpace <= 0;
    if (m_tries++ % 10 == 0 || !freeSpaceIsInvalid)
    {
        m_deviceSpacesCache[fsPath].freeSpace =
            m_rootFs->freeSpace(QString::fromLatin1(fsPath));
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
    auto fileStats = m_rootFs->stat(fsPath);
    if (!fileStats.exists)
        return false;

    return fileStats.type == SystemCommands::Stats::FileType::directory;
}

QStringList PartitionsInformationProvider::additionalLocalFsTypes() const
{
    const QString settingsValue = m_globalSettings->additionalLocalFsTypes();
    NX_MUTEX_LOCKER lock(&m_fsTypesMutex);
    if (settingsValue != m_additionalFsTypesString)
    {
        m_additionalFsTypes.clear();
        const auto splits = settingsValue.split(",", QString::SkipEmptyParts);
        std::transform(
            splits.cbegin(), splits.cend(), std::back_inserter(m_additionalFsTypes),
            [](const auto& s) { return s.trimmed(); });

    }
    return m_additionalFsTypes;
}

} // namesapce nx::vms::server::fs
