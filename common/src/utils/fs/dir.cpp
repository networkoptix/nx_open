/**********************************************************
* Oct 5, 2015
* a.kolesnikov
***********************************************************/

#include "dir.h"

#include <map>
#include <memory>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <net/if_arp.h>
#include <sys/statvfs.h>
#include <nx/utils/app_info.h>
#endif


static const size_t MAX_LINE_LENGTH = 512;

SystemError::ErrorCode readPartitions(
    std::list<PartitionInfo>* const partitionInfoList)
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
