#ifndef __QN_ABSTRACT_MOTION_ARCHIVE_H__
#define __QN_ABSTRACT_MOTION_ARCHIVE_H__

class QnAbstractMotionArchiveConnection
{
public:
    /*
    * Return metadaTata in motion archive for specified time. If next call got same metadata or no metada is found return null.
    */
    virtual QnMetaDataV1Ptr getMotionData(qint64 timeUsec) = 0;
};

#endif // __QN_ABSTRACT_MOTION_ARCHIVE_H__

typedef QSharedPointer<QnAbstractMotionArchiveConnection> QnAbstractMotionArchiveConnectionPtr;
