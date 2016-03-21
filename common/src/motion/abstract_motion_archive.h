#ifndef __QN_ABSTRACT_MOTION_ARCHIVE_H__
#define __QN_ABSTRACT_MOTION_ARCHIVE_H__

#ifdef ENABLE_DATA_PROVIDERS

#include "core/datapacket/media_data_packet.h"

class QnAbstractMotionArchiveConnection
{
public:
    /*
    * Return metadaTata in motion archive for specified time. If next call got same metadata or no metada is found return null.
    */
    virtual QnMetaDataV1Ptr getMotionData(qint64 timeUsec) = 0;

    virtual ~QnAbstractMotionArchiveConnection() {}
};
typedef QSharedPointer<QnAbstractMotionArchiveConnection> QnAbstractMotionArchiveConnectionPtr;

class QnAbstractMotionConnectionFactory
{
public:
    virtual QnAbstractMotionArchiveConnectionPtr createMotionArchiveConnection() = 0;
};

typedef QSharedPointer<QnAbstractMotionConnectionFactory> QnAbstractMotionConnectionFactoryPtr;

#endif // ENABLE_DATA_PROVIDERS

#endif // __QN_ABSTRACT_MOTION_ARCHIVE_H__
