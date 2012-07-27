#ifndef __MOTION_HELPER_H__
#define __MOTION_HELPER_H__

#include <QObject>
#include <QRegion>
#include <QMap>
#include "utils/media/sse_helper.h"
#include "core/datapacket/mediadatapacket.h"
#include "core/resource/network_resource.h"
#include "recorder/device_file_catalog.h"
#include "motion_archive.h"
#include "recording/time_period_list.h"

class QnMotionHelper
{
public:

    static QnMotionHelper* instance();
    virtual ~QnMotionHelper();

    // write motion data to file
    void saveToArchive(QnMetaDataV1Ptr data);

    QnTimePeriodList mathImage(const QList<QRegion>& region, QnResourceList resList, qint64 msStartTime, qint64 msEndTime, int detailLevel);
    QnTimePeriodList mathImage(const QList<QRegion>& region, QnResourcePtr res, qint64 msStartTime, qint64 msEndTime, int detailLevel);
    QnMotionArchiveConnectionPtr createConnection(QnResourcePtr res, int channel);

    QnMotionArchive* getArchive(QnResourcePtr res, int channel);

    static QString getMotionDir(const QDate& date, const QString& macAddress);
    static void deleteUnusedFiles(const QList<QDate>& chunks, const QString& macAddress);
    static QList<QDate> recordedMonth(const QString& macAddress);

    QnMotionHelper();
private:
    static QString getBaseDir(const QString& macAddress);

    // create Find mask by region
    void createMask(const QRegion& region);

    // mach one motion image by mask
    bool mathImage(const __m128i* data);

private:
    typedef QPair<QnNetworkResourcePtr, int> MotionArchiveKey;
    typedef QMap<MotionArchiveKey, QnMotionArchive*> MotionWriters;
    MotionWriters m_writers;
    QMutex m_mutex;
};


#endif // __MOTION_HELPER_H__
