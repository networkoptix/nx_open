#include "motion_archive.h"
#include <QDateTime>
#include <QDir>
#include "utils/common/util.h"

static const char version = 1;
static const quint16 DETAILED_AGGREGATE_INTERVAL = 5; // at seconds
static const quint16 COARSE_AGGREGATE_INTERVAL = 3600; // at seconds

#pragma pack(push, 1)
struct IndexRecord
{
    int start;    // at ms since epoch + start offset from a file header
    int duration; // at ms
};
bool operator < (const IndexRecord& first, const IndexRecord& other) { return first.start < other.start; }
bool operator < (qint64 start, const IndexRecord& other) { return start < other.start; }
bool operator < (const IndexRecord& other, qint64 start) { return other.start < start;}


struct IndexHeader
{
    qint64 startTime;
    qint16 interval;
    qint8 version;
    char dummy[5];
};
#pragma pack(pop)


QnMotionArchive::QnMotionArchive(QnNetworkResourcePtr resource): 
    m_resource(resource),
    m_lastDetailedData(new QnMetaDataV1())
{
    m_lastDateForCurrentFile = 0;
    m_firstTime = 0;
}

QString QnMotionArchive::getFilePrefix(const QDateTime& datetime)
{
    return closeDirPath(getDataDirectory()) + QString("record_catalog/metadata/") + m_resource->getMAC().toString() + QString("/") + datetime.toString("yyyy/MM/");
}

void QnMotionArchive::fillFileNames(const QDateTime& datetime, QFile& motionFile, QFile& indexFile)
{
    QString fileName = getFilePrefix(datetime);
    motionFile.setFileName(fileName+"motion_detailed_data.bin");
    indexFile.setFileName(fileName+"motion_detailed_index.bin");
    QMutex m_fileAccessMutex;
    QDir dir;
    dir.mkpath(fileName);
}

inline void setBit(quint8* data, int x, int y)
{
    int offset = (x * MD_HIGHT + y) / 8;
    data[offset] |= 0x80 >> (y&7);
}

void QnMotionArchive::createMask(const QRegion& region,  __m128i* mask, int& maskStart, int& maskEnd)
{

    maskStart = 0;
    maskEnd = 0;
    memset(mask, 0, MD_WIDTH * MD_HIGHT / 8);

    for (int i = 0; i < region.rectCount(); ++i)
    {
        const QRect& rect = region.rects().at(i);
        maskStart = qMin((rect.left() * MD_HIGHT + rect.top()) / 128, maskStart);
        maskEnd = qMax((rect.right() * MD_HIGHT + rect.bottom()) / 128, maskEnd);
        for (int x = rect.left(); x <= rect.right(); ++x)
        {
            for (int y = rect.top(); y <= rect.bottom(); ++y)
                setBit((quint8*) mask, x,y);
        }
    }

}

bool QnMotionArchive::mathImage(const __m128i* data, const __m128i* mask, int maskStart, int maskEnd)
{
    for (int i = maskStart; i <= maskEnd; ++i)
    {
        if (_mm_testz_si128(mask[i], data[i]) == 0)
            return true;
    }
    return false;
}

QnTimePeriodList QnMotionArchive::mathPeriod(const QRegion& region, qint64 msStartTime, qint64 msEndTime)
{
    QnTimePeriodList rez;
    QFile motionFile, indexFile;
    //qint64 msStartTime = startTime/1000; // convert from usec to ms
    //qint64 msEndTime = endTime/1000; // convert from usec to ms
    quint8* buffer = (quint8*) qMallocAligned(MOTION_DATA_RECORD_SIZE * 1024, 32);
    __m128i mask[MD_WIDTH * MD_HIGHT / 128];
    int maskStart, maskEnd;
    createMask(region, mask, maskStart, maskEnd);
    bool isFirstStep = true;

    while (msStartTime < msEndTime)
    {
        QDateTime qmsStartTime = QDateTime::fromMSecsSinceEpoch(msStartTime);
        fillFileNames(qmsStartTime, motionFile, indexFile);
        
        if (!motionFile.open(QFile::ReadOnly) || !indexFile.open(QFile::ReadOnly)) {
            qFreeAligned(buffer);
            return rez;
        }

        QVector<IndexRecord> index;
        qint64 indexSize = (indexFile.size()-MOTION_INDEX_HEADER_SIZE);
        if (indexSize == 0)
            return rez;
        index.resize(indexSize/MOTION_INDEX_RECORD_SIZE);
        IndexHeader indexHeader;
        indexFile.read((char*) &indexHeader, MOTION_INDEX_HEADER_SIZE);
        indexFile.read((char*) &index[0], indexSize);

        qint64 minTime, maxTime;
        dateBounds(qmsStartTime, minTime, maxTime);

        QVector<IndexRecord>::iterator startItr = index.begin();
        QVector<IndexRecord>::iterator endItr = index.end();

        if (isFirstStep)
            startItr = qLowerBound(index.begin(), index.end(), msStartTime-indexHeader.startTime);
        if (maxTime <= msEndTime)
            endItr = qUpperBound(startItr, index.end(), msEndTime-indexHeader.startTime);
        int totalSteps = endItr - startItr;

        motionFile.seek((startItr-index.begin()) * MOTION_DATA_RECORD_SIZE);

        // math file (one month)
        while (totalSteps > 0)
        {
            int readed = motionFile.read((char*) buffer, MOTION_DATA_RECORD_SIZE * qMin(totalSteps,1024));
            if (readed <= 0)
                break;
            quint8* dataEnd = buffer + readed;
            quint8* curData = buffer;
            for (QVector<IndexRecord>::const_iterator i = startItr; i < endItr && curData < dataEnd; ++i)
            {
                if (mathImage((__m128i*) curData, mask, maskStart, maskEnd))
                {
                    qint64 fullStartTime = i->start + minTime;
                    if (fullStartTime >= msEndTime) {
                        totalSteps = 0;
                        break;
                    }
                    if (!rez.isEmpty() && fullStartTime <= rez.last().startTimeUSec + rez.last().durationUSec)
                        rez.last().durationUSec = qMax(rez.last().durationUSec, i->duration + fullStartTime - rez.last().startTimeUSec);
                    else
                        rez.push_back(QnTimePeriod(fullStartTime, i->duration));
                }
                curData += MOTION_DATA_RECORD_SIZE;
            }
            totalSteps -= readed/MOTION_DATA_RECORD_SIZE;
        }
        msStartTime = maxTime + 1;
        isFirstStep = false;
    }

    qFreeAligned(buffer);
    return rez;
}

void QnMotionArchive::dateBounds(const QDateTime& datetime, qint64& minDate, qint64& maxDate)
{
    QDateTime monthStart(datetime.date().addDays(1-datetime.date().day()));
    minDate = monthStart.toMSecsSinceEpoch();

    QDateTime nextMonth(monthStart.addMonths(1));
    maxDate = nextMonth.toMSecsSinceEpoch()-1;
}

bool QnMotionArchive::saveToArchiveInternal(QnMetaDataV1Ptr data)
{
    qint64 timestamp = data->timestamp/1000;
    QDateTime datetime = QDateTime::fromMSecsSinceEpoch(timestamp);
    if (timestamp > m_lastDateForCurrentFile)
    {

        dateBounds(datetime, m_firstTime, m_lastDateForCurrentFile);

        //QString fileName = getFilePrefix(datetime);
        m_detailedMotionFile.close();
        m_detailedIndexFile.close();
        fillFileNames(datetime, m_detailedMotionFile, m_detailedIndexFile);
        if (!m_detailedMotionFile.open(QFile::WriteOnly | QFile::Append))
            return false;

        if (!m_detailedIndexFile.open(QFile::WriteOnly | QFile::Append))
            return false;

        // truncate biggest file. So, it is error checking
        int indexRecords = qMax((m_detailedIndexFile.size() - MOTION_INDEX_HEADER_SIZE) / MOTION_INDEX_RECORD_SIZE, 0ll);
        int dataRecords  = m_detailedMotionFile.size() / MOTION_DATA_RECORD_SIZE;
        if (indexRecords > dataRecords) {
            QMutexLocker lock(&m_fileAccessMutex);
            if (!m_detailedIndexFile.resize(dataRecords*MOTION_INDEX_RECORD_SIZE + MOTION_INDEX_HEADER_SIZE))
                return false;
        }
        else if ((indexRecords < dataRecords)) {
            QMutexLocker lock(&m_fileAccessMutex);
            if (!m_detailedMotionFile.resize(indexRecords*MOTION_DATA_RECORD_SIZE))
                return false;
        }

        if (m_detailedIndexFile.size() == 0)
        {
            IndexHeader header;
            header.version = version;
            header.interval = DETAILED_AGGREGATE_INTERVAL;
            header.startTime = m_firstTime;
            m_detailedIndexFile.write((const char*) &header, sizeof(IndexHeader));
        }
    }
    int relTime = int(timestamp - m_firstTime);
    int duration = int(data->m_duration/1000);
    m_detailedIndexFile.write((const char*) &relTime, 4);
    m_detailedIndexFile.write((const char*) &duration, 4);

    m_detailedMotionFile.write(data->data.constData(), data->data.capacity());

    m_detailedIndexFile.flush();
    m_detailedMotionFile.flush();
    return true;
}

bool QnMotionArchive::saveToArchive(QnMetaDataV1Ptr data)
{
    bool rez = true;
    if (data->timestamp - m_lastDetailedData->timestamp < DETAILED_AGGREGATE_INTERVAL*1000000ll)
    {
        // aggregate data
        m_lastDetailedData->addMotion(data);
    }
    else {
        // save to disk
        m_lastDetailedData->m_duration = data->timestamp - m_lastDetailedData->timestamp;
        rez = saveToArchiveInternal(m_lastDetailedData);
        m_lastDetailedData = data;
    }
    return rez;
}
