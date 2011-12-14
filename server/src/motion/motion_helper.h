#ifndef __MOTION_HELPER_H__
#define __MOTION_HELPER_H__

#include <QObject>
#include <QRegion>
#include <QMap>
#include "utils/media/sse_helper.h"
#include "core/datapacket/mediadatapacket.h"
#include "core/resource/network_resource.h"
#include "recording/device_file_catalog.h"
#include "motion_archive.h"

class QnMotionHelper
{
public:

    static QnMotionHelper* instance();
    virtual ~QnMotionHelper();

    // write motion data to file
    void saveToArchive(QnMetaDataV1Ptr data);

    QnTimePeriodList mathImage(const QRegion& region, QnResourceList resList, qint64 msStartTime, qint64 msEndTime);
    QnTimePeriodList mathImage(const QRegion& region, QnResourcePtr res, qint64 msStartTime, qint64 msEndTime);
    QnMotionArchiveConnectionPtr createConnection(QnResourcePtr res);

    QnMotionHelper();
private:
    // create Find mask by region
    void createMask(const QRegion& region);

    // mach one motion image by mask
    bool mathImage(const __m128i* data);

    QnMotionArchive* getArchive(QnResourcePtr res);
private:
    typedef QMap<QnNetworkResourcePtr, QnMotionArchive*> MotionWriters;
    MotionWriters m_writers;
    QMutex m_mutex;
};

#endif // __MOTION_HELPER_H__
