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

    // remove part of motion by mask
    void maskMotion(QnMetaDataV1Ptr data);

    // write motion data to file
    void saveToArchive(QnMetaDataV1Ptr data);

    QnTimePeriodList mathImage(const QRegion& region, QnResourceList resList, qint64 msStartTime, qint64 msEndTime, int detailLevel);
    QnTimePeriodList mathImage(const QRegion& region, QnResourcePtr res, qint64 msStartTime, qint64 msEndTime, int detailLevel);
    QnMotionArchiveConnectionPtr createConnection(QnResourcePtr res);

    static QString getMotionDir(const QDate& date, const QString& macAddress);
    static void deleteUnusedFiles(const QList<QDate>& chunks, const QString& macAddress);

    QnMotionHelper();
private:
    static QString getBaseDir(const QString& macAddress);

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
