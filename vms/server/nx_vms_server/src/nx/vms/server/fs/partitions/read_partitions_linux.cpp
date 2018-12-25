#include "read_partitions_linux.h"

#include <map>
#include <memory>
#include <QtCore>

#include <time.h>
#include <signal.h>
#include <errno.h>
#include <net/if_arp.h>
#include <sys/statvfs.h>
#include <nx/utils/app_info.h>
#include <nx/utils/literal.h>
#include <nx/utils/file_system.h>
#include <nx/vms/server/fs/partitions/abstract_partitions_information_provider_linux.h>

namespace nx::vms::server::fs {

namespace { static const int kMaxLineLength = 512; }

SystemError::ErrorCode readPartitionsInformation(
    AbstractPartitionsInformationProvider* partitionsInfoProvider,
    std::list<PartitionInfo>* const partitionInfoList)
{
    QByteArray fileContent = partitionsInfoProvider->mountsFileContent();
    if (fileContent.isNull())
        return SystemError::fileNotFound;

    QBuffer file(&fileContent);
    file.open(QIODevice::ReadOnly);

    enum {fsPath, fsType};
    QMap<QByteArray, std::pair<QByteArray, QByteArray>> deviceToPath;

    for (QByteArray line = file.readLine(); !line.isNull(); line = file.readLine())
    {
        char cDevName[kMaxLineLength];
        char cPath[kMaxLineLength];
        char cFSName[kMaxLineLength];
        if (partitionsInfoProvider->isScanfLongPattern())
        {
            char cTmp[kMaxLineLength];
            if (sscanf(line.constData(), "%s %s %s %s %s ", cDevName, cTmp, cPath, cTmp, cFSName) != 5)
                continue; /* Skip unrecognized lines. */
        }
        else
        {
            if (sscanf(line.constData(), "%s %s %s ", cDevName, cPath, cFSName) != 3)
                continue; /* Skip unrecognized lines. */
        }

        decodeOctalEncodedPath(cPath);

        const bool suitableForInsert = !deviceToPath.contains(cDevName)
            || (int) strlen(cPath) < std::get<fsPath>(deviceToPath[cDevName]).size();

        if (suitableForInsert && partitionsInfoProvider->isFolder(cPath))
            deviceToPath[cDevName] = std::make_pair(cPath, cFSName);
    }

    for (auto deviceKey: deviceToPath.keys())
    {
        const auto& pathInfo = deviceToPath[deviceKey];
        PartitionInfo partitionInfo;

        partitionInfo.isUsb = nx::utils::file_system::isUsb(QString::fromLatin1(deviceKey));
        partitionInfo.devName = QString::fromLatin1(deviceKey);
        partitionInfo.path = QString::fromLatin1(std::get<fsPath>(pathInfo));

        partitionInfo.fsName = QString::fromLatin1(std::get<fsType>(pathInfo));
        partitionInfo.sizeBytes = partitionsInfoProvider->totalSpace(std::get<fsPath>(pathInfo));
        if (partitionInfo.sizeBytes == -1)
            continue;

        partitionInfo.freeBytes = partitionsInfoProvider->freeSpace(std::get<fsPath>(pathInfo));
        if (partitionInfo.freeBytes == -1)
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

} // namespace nx::vms::server::fs
