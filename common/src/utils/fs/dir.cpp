/**********************************************************
* Oct 5, 2015
* a.kolesnikov
***********************************************************/

#include "dir.h"

#include <map>
#include <memory>
#include <QtCore>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <net/if_arp.h>
#include <sys/statvfs.h>
#include <nx/utils/app_info.h>
#include <nx/utils/literal.h>
#include <nx/utils/file_system.h>
#endif


static const size_t MAX_LINE_LENGTH = 512;

SystemError::ErrorCode readPartitions(std::list<PartitionInfo>* const partitionInfoList)
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    std::string mountsFile("/proc/mounts");
    bool scanfLongPattern = false;
    if (nx::utils::AppInfo::isRaspberryPi())
    {
        // On Rapberry Pi 2+ "/proc/mounts" contains unexisting "/dev/root" on "/", while mount
        // reports a valid mounted device.
        if (system("mount > /root/mounts") == 0)
        {
            // Some devices have /tmp related problems, while root can always access /root.
            mountsFile = "/root/mounts";
            scanfLongPattern = true;
        }
        else if (system("mount > /tmp/mounts") == 0)
        {
            // Temporary directory /tmp is a fallback for non-root users.
            mountsFile = "/tmp/mounts";
            scanfLongPattern = true;
        }
    }

    std::map<QString, std::pair<QString, QString>> deviceToPath;
    std::unique_ptr<FILE, decltype(&fclose)> file(fopen(mountsFile.c_str(), "r"), fclose);
    if (!file)
        return SystemError::getLastOSErrorCode();

    char line[MAX_LINE_LENGTH];
    for (int i = 0; fgets(line, MAX_LINE_LENGTH, file.get()) != NULL; ++i)
    {
        if (i == 0 && !scanfLongPattern)
            continue; /* Skip header. */

        char cDevName[MAX_LINE_LENGTH];
        char cPath[MAX_LINE_LENGTH];
        char cFSName[MAX_LINE_LENGTH];
        if (scanfLongPattern)
        {
            char cTmp[MAX_LINE_LENGTH];
            if (sscanf(line, "%s %s %s %s %s ", cDevName, cTmp, cPath, cTmp, cFSName) != 5)
                continue; /* Skip unrecognized lines. */
        }
        else
        {
            if (sscanf(line, "%s %s %s ", cDevName, cPath, cFSName) != 3)
                continue; /* Skip unrecognized lines. */
        }

        decodeOctalEncodedPath(cPath);

        const QString& devName = QString::fromUtf8(cDevName);
        const QString& path = QString::fromUtf8(cPath);
        const QString& fsName = QString::fromUtf8(cFSName);

        auto p = deviceToPath.emplace(devName, std::make_pair(path, fsName));
        if (!p.second)
        {
            //device has mutiple mount points
            if (path.length() < p.first->second.first.length())
            {
                p.first->second.first = path; //selecting shortest mount point
                p.first->second.second = fsName;
            }
        }
    }

    for (auto deviceAndPath: deviceToPath)
    {
        struct statvfs64 vfsInfo;
        memset(&vfsInfo, 0, sizeof(vfsInfo));
        if (statvfs64(deviceAndPath.second.first.toUtf8().constData(), &vfsInfo) == -1)
            continue;

        PartitionInfo partitionInfo;
        partitionInfo.isUsb = nx::utils::file_system::isUsb(deviceAndPath.first);
        partitionInfo.devName = deviceAndPath.first;
        partitionInfo.path = deviceAndPath.second.first;
        partitionInfo.fsName = deviceAndPath.second.second;
        partitionInfo.sizeBytes = (int64_t)vfsInfo.f_blocks * vfsInfo.f_frsize;
        partitionInfo.freeBytes = (int64_t)vfsInfo.f_bfree * vfsInfo.f_bsize;
        partitionInfoList->emplace_back(std::move(partitionInfo));
    }

    return SystemError::noError;
#else
    Q_UNUSED(partitionInfoList);
    return SystemError::notImplemented;
#endif
}

QString mountsFileName(bool* scanfLongPattern)
{
    QString mountsFileName = lit("/proc/mounts");
    *scanfLongPattern = false;
    if (nx::utils::AppInfo::isRaspberryPi())
    {
        // On Rapberry Pi 2+ "/proc/mounts" contains unexisting "/dev/root" on "/", while mount
        // reports a valid mounted device.
        if (system("mount > /root/mounts") == 0)
        {
            // Some devices have /tmp related problems, while root can always access /root.
            mountsFileName = lit("/root/mounts");
            *scanfLongPattern = true;
        }
        else if (system("mount > /tmp/mounts") == 0)
        {
            // Temporary directory /tmp is a fallback for non-root users.
            mountsFileName = lit("/tmp/mounts");
            *scanfLongPattern = true;
        }
    }

    return mountsFileName;
}

SystemError::ErrorCode readPartitions(
    AbstractSystemInfoProvider* systemInfoProvider,
    std::list<PartitionInfo>* const partitionInfoList)
{
    struct PathInfo
    {
        QByteArray fsPath;
        QByteArray fsType;

        PathInfo(const QByteArray& fsPath, const QByteArray& fsType):
            fsPath(fsPath), fsType(fsType)
        {}

        PathInfo() = default;
    };

    QByteArray fileContent = systemInfoProvider->fileContent();
    if (fileContent.isNull())
        return SystemError::fileNotFound;

    QBuffer file(&fileContent);
    file.open(QIODevice::ReadOnly);
    if (!systemInfoProvider->scanfLongPattern())
        file.readLine();

    QMap<QByteArray, PathInfo> deviceToPath;
    QByteArray line;

    for (line = file.readLine(); !line.isNull(); line = file.readLine())
    {
        char cDevName[MAX_LINE_LENGTH];
        char cPath[MAX_LINE_LENGTH];
        char cFSName[MAX_LINE_LENGTH];
        if (systemInfoProvider->scanfLongPattern())
        {
            char cTmp[MAX_LINE_LENGTH];
            if (sscanf(line.constData(), "%s %s %s %s %s ", cDevName, cTmp, cPath, cTmp, cFSName) != 5)
                continue; /* Skip unrecognized lines. */
        }
        else
        {
            if (sscanf(line.constData(), "%s %s %s ", cDevName, cPath, cFSName) != 3)
                continue; /* Skip unrecognized lines. */
        }

        decodeOctalEncodedPath(cPath);

        if (!deviceToPath.contains(cDevName) || strlen(cPath) < deviceToPath[cDevName].fsPath.size())
            deviceToPath.insert(cDevName, PathInfo(cPath, cFSName));
    }

    for (auto deviceKey: deviceToPath.keys())
    {
        const PathInfo& pathInfo = deviceToPath[deviceKey];
        PartitionInfo partitionInfo;

        partitionInfo.isUsb = nx::utils::file_system::isUsb(QString::fromLatin1(deviceKey));
        partitionInfo.devName = QString::fromLatin1(deviceKey);
        partitionInfo.path = QString::fromLatin1(pathInfo.fsPath);
        partitionInfo.fsName = QString::fromLatin1(pathInfo.fsType);
        partitionInfo.freeBytes = systemInfoProvider->freeSpace(pathInfo.fsPath);
        partitionInfo.sizeBytes = systemInfoProvider->totalSpace(pathInfo.fsPath);

        if (partitionInfo.freeBytes == -1 || partitionInfo.sizeBytes == -1)
            continue;

        partitionInfoList->emplace_back(std::move(partitionInfo));
    }

    return SystemError::noError;
}

void decodeOctalEncodedPath(char* path)
{
    for (char* cPathPtr = path; *cPathPtr != '\0'; ++cPathPtr)
    {
        if (*cPathPtr == '\\' && *(cPathPtr + 1) == '0')
        {
            unsigned long charCode;
            char* patternEnd;
            if ((charCode = strtoul(cPathPtr + 1, &patternEnd, 8)) == 0)
                continue;

            *cPathPtr = (char) charCode;
            char* ptrCopy = cPathPtr + 1;

            for (; *patternEnd != '\0'; ++patternEnd)
                *ptrCopy++ = *patternEnd;

            *ptrCopy = '\0';
        }
    }
}

CommonSystemInfoProvider::CommonSystemInfoProvider()
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

QByteArray CommonSystemInfoProvider::fileContent() const
{
    QFile mountsFile(m_fileName);
    if (!mountsFile.open(QIODevice::ReadOnly))
        return QByteArray();

    return mountsFile.readAll();
}

bool CommonSystemInfoProvider::scanfLongPattern() const
{
    return m_scanfLongPattern;
}

qint64 CommonSystemInfoProvider::totalSpace(const QByteArray& fsPath) const
{
    struct statvfs64 stat;
    if (statvfs64(fsPath.constData(), &stat) == 0)
       return stat.f_blocks * (qint64) stat.f_frsize;

    return -1LL;
}

qint64 CommonSystemInfoProvider::freeSpace(const QByteArray& fsPath) const
{
    struct statvfs64 stat;
    if (statvfs64(fsPath.constData(), &stat) == 0)
        return stat.f_bavail * (int64_t) stat.f_bsize;

    return -1LL;
}

QString CommonSystemInfoProvider::fileName() const
{
    return m_fileName;
}
