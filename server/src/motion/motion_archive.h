#ifndef __MOTION_WRITER_H__
#define __MOTION_WRITER_H__

#include <QRegion>
#include <QFile>
#include "core/resource/network_resource.h"
#include "core/datapacket/mediadatapacket.h"
#include "utils/media/sse_helper.h"
#include "recording/device_file_catalog.h"

static const int MOTION_INDEX_HEADER_SIZE = 16;
static const int MOTION_INDEX_RECORD_SIZE = 8;
static const int MOTION_DATA_RECORD_SIZE = MD_WIDTH*MD_HIGHT/8;

class QnMotionArchive
{
public:
    QnMotionArchive(QnNetworkResourcePtr resource);
    bool saveToArchive(QnMetaDataV1Ptr data);
    QnTimePeriodList mathPeriod(const QRegion& region, qint64 startTime, qint64 endTime);

    static void createMask(const QRegion& region,  __m128i* mask, int& msMaskStart, int& msMaskEnd);
private:
    QString getFilePrefix(const QDateTime& datetime);
    void dateBounds(const QDateTime& datetime, qint64& minDate, qint64& maxDate);
    bool mathImage(const __m128i* data, const __m128i* mask, int maskStart, int maskEnd);
    void fillFileNames(const QDateTime& datetime, QFile& motionFile, QFile& indexFile);
    bool saveToArchiveInternal(QnMetaDataV1Ptr data);
private:
    QnNetworkResourcePtr m_resource;
    qint64 m_lastDateForCurrentFile;
    
    QFile m_detailedMotionFile;
    QFile m_detailedIndexFile;
    qint64 m_firstTime;
    __m128i m_mask[MD_WIDTH * MD_HIGHT / 128];
    QMutex m_fileAccessMutex;
    QnMetaDataV1Ptr m_lastDetailedData;
};

#endif // __MOTION_WRITER_H__
