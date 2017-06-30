/**********************************************************
* Oct 5, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_DIR_H
#define NX_DIR_H

#include <list>

#include <nx/utils/system_error.h>


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
};

SystemError::ErrorCode readPartitions(
    std::list<PartitionInfo>* const partitionInfoList);

#endif  //NX_DIR_H
