#ifndef __LIGHT_MOTION_ARCHIVE_CONNECTION_H__
#define __LIGHT_MOTION_ARCHIVE_CONNECTION_H__

#ifdef ENABLE_DATA_PROVIDERS

#include "abstract_motion_archive.h"

class QnLightMotionArchiveConnection: public QnAbstractMotionArchiveConnection
{
public:
    QnLightMotionArchiveConnection(const QnMetaDataLightVector& data, int channel);
    virtual QnMetaDataV1Ptr getMotionData(qint64 timeUsec) override;
private:
    const QnMetaDataLightVector m_motionData;
    int m_channel;
    QnMetaDataV1Ptr m_lastResult;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // __LIGHT_MOTION_ARCHIVE_CONNECTION_H__
