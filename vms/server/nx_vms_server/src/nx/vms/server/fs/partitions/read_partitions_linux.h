#pragma once

#include <list>

#include <nx/utils/system_error.h>

namespace nx::vms::server::fs {

class AbstractPartitionsInformationProvider;

struct PartitionInfo
{
    PartitionInfo()
    :
        freeBytes(0),
        sizeBytes(0)
    {
    }

    /** System-dependent name of device */
    QString devName;

    /** Partition's root path. */
    QString path;

    /** Partition's filesystem name. */
    QString fsName;

    /** Free space of this partition in bytes */
    qint64 freeBytes;

    /** Total size of this partition in bytes */
    qint64 sizeBytes;

    bool isUsb = false;
};

SystemError::ErrorCode readPartitionsInformation(
    AbstractPartitionsInformationProvider* partitionsInfoProvider,
    std::list<PartitionInfo>* const partitionInfoList);

void decodeOctalEncodedPath(char* path);

} // namespace nx::vms::server::fs
