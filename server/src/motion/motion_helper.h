#ifndef __MOTION_HELPER_H__
#define __MOTION_HELPER_H__

#include <QObject>
#include <QRegion>
#include <QMap>
#include "utils/media/sse_helper.h"
#include "core/datapacket/mediadatapacket.h"
#include "core/resource/network_resource.h"
#include "recording/device_file_catalog.h"

class QnMotionArchive;

class QnMotionHelper
{
public:

    static QnMotionHelper* instance();
    virtual ~QnMotionHelper();

    // write motion data to file
    void saveToArchive(QnMetaDataV1Ptr data);

    QnTimePeriodList mathImage(const QRegion& region, QnResourceList resList, qint64 startTime, qint64 endTime);
    QnTimePeriodList mathImage(const QRegion& region, QnResourcePtr res, qint64 startTime, qint64 endTime);

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
