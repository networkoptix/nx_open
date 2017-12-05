#include "motion_archive.h"
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include "utils/common/util.h"
#include "motion_helper.h"

#include <core/resource/security_cam_resource.h>

#include <recording/time_period.h>
#include <recording/time_period_list.h>

#ifdef Q_OS_MAC
#include <smmintrin.h>
#endif

static const char version = 1;
static const quint16 DETAILED_AGGREGATE_INTERVAL = 3; // at seconds
//static const quint16 COARSE_AGGREGATE_INTERVAL = 3600; // at seconds

bool operator < (const IndexRecord& first, const IndexRecord& other) { return first.start < other.start; }
bool operator < (qint64 start, const IndexRecord& other) { return start < other.start; }
bool operator < (const IndexRecord& other, qint64 start) { return other.start < start;}

// --------------- QnMotionArchiveConnection ---------------------

QnMotionArchiveConnection::QnMotionArchiveConnection(QnMotionArchive* owner):
    m_owner(owner),
    m_minDate(AV_NOPTS_VALUE),
    m_maxDate(AV_NOPTS_VALUE),
    m_lastResult(new QnMetaDataV1())
{
    m_lastTimeMs = AV_NOPTS_VALUE;
    m_motionLoadedStart = m_motionLoadedEnd = -1;
    m_motionBuffer = (quint8*) qMallocAligned(1024 * MOTION_DATA_RECORD_SIZE, 32);
}

QnMotionArchiveConnection::~QnMotionArchiveConnection()
{
    qFreeAligned(m_motionBuffer);
}

QnAbstractCompressedMetadataPtr QnMotionArchiveConnection::getMotionData(qint64 timeUsec)
{
    if (m_lastResult->containTime(timeUsec))
        return QnMetaDataV1Ptr(); // do not duplicate data

    qint64 timeMs = timeUsec/1000;

    if (m_index.isEmpty() || timeMs < m_index.front().start + m_indexHeader.startTime ||
        timeMs > m_index.last().start + +m_index.last().duration + m_indexHeader.startTime)
    {
        qint64 prevMaxDate = m_maxDate;
        m_owner->dateBounds(timeMs, m_minDate, m_maxDate);
        if (prevMaxDate != m_maxDate)
        {
            m_lastTimeMs = AV_NOPTS_VALUE; // reset value. We are going to find iterator from vector begin() next time
            m_motionLoadedStart = m_motionLoadedEnd = -1;
            if (!m_owner->loadIndexFile(m_index, m_indexHeader, timeMs))
            {
                m_maxDate = AV_NOPTS_VALUE;
                return QnMetaDataV1Ptr();
            }
            m_maxDate = qMin(m_maxDate, QDateTime::currentDateTime().toMSecsSinceEpoch());
        }
    }

    if (m_lastTimeMs != (qint64)AV_NOPTS_VALUE && timeMs > m_lastTimeMs)
        m_indexItr = std::upper_bound(m_indexItr, m_index.end(), timeMs - m_indexHeader.startTime);
    else
        m_indexItr = std::upper_bound(m_index.begin(), m_index.end(), timeMs - m_indexHeader.startTime);

    if (m_indexItr > m_index.begin())
        m_indexItr--;

    qint64 foundStartTime = m_indexItr->start + m_indexHeader.startTime;
    bool isMotionFound = foundStartTime <= timeMs && foundStartTime + m_indexItr->duration > timeMs;
    if (!isMotionFound)
        return QnMetaDataV1Ptr();

    int indexOffset = m_indexItr - m_index.begin();
    bool isMotionLoaded = m_motionLoadedStart <= indexOffset && m_motionLoadedEnd > indexOffset;
    if (!isMotionLoaded)
    {
        m_owner->fillFileNames(timeMs, &m_motionFile, 0);
        if (m_motionFile.open(QFile::ReadOnly))
        {
            m_motionLoadedStart = qMax(0, indexOffset - 512);
            m_motionFile.seek(m_motionLoadedStart * MOTION_DATA_RECORD_SIZE);
            int readed = qMax(0ll, m_motionFile.read((char*) m_motionBuffer, 1024 * MOTION_DATA_RECORD_SIZE));
            m_motionLoadedEnd = m_motionLoadedStart + readed / MOTION_DATA_RECORD_SIZE;
            isMotionLoaded = m_motionLoadedStart <= indexOffset && m_motionLoadedEnd > indexOffset;
        }
    }
    if (!isMotionLoaded)
        return QnMetaDataV1Ptr();


    m_lastResult = QnMetaDataV1Ptr(new QnMetaDataV1());
    m_lastResult->m_data.clear();
    int motionIndex = indexOffset - m_motionLoadedStart;
    m_lastResult->m_data.write((const char*) m_motionBuffer + motionIndex*MOTION_DATA_RECORD_SIZE, MOTION_DATA_RECORD_SIZE);
    m_lastResult->timestamp = foundStartTime*1000;
    m_lastResult->m_duration = m_indexItr->duration*1000;
    m_lastResult->channelNumber = m_owner->getChannel();

    //qDebug() << "motion=" << QDateTime::fromMSecsSinceEpoch(m_lastResult->timestamp/1000).toString("hh.mm.ss.zzz")
    //         << "dur=" << m_lastResult->m_duration/1000.0
    //         <<  "reqTime=" << QDateTime::fromMSecsSinceEpoch(timeMs).toString("hh.mm.ss.zzz");

    return m_lastResult;
}

// ----------------------- QnMotionArchive ------------------

QnMotionArchive::QnMotionArchive(QnNetworkResourcePtr resource, int channel):
m_resource(resource),
m_channel(channel),
m_lastDetailedData(new QnMetaDataV1()),
m_lastTimestamp(AV_NOPTS_VALUE),
m_middleRecordNum(-1)
{
    m_camResource = qSharedPointerDynamicCast<QnSecurityCamResource>(m_resource);
    m_lastDateForCurrentFile = 0;
    m_firstTime = 0;
    loadRecordedRange();
}

QnMotionArchive::~QnMotionArchive()
{
}

void QnMotionArchive::loadRecordedRange()
{
    m_minMotionTime = AV_NOPTS_VALUE;
    m_maxMotionTime = AV_NOPTS_VALUE;
    m_lastRecordedTime = AV_NOPTS_VALUE;
    QList<QDate> existsRecords = QnMotionHelper::instance()->recordedMonth(m_resource->getUniqueId());
    if (existsRecords.isEmpty())
        return;

    QVector<IndexRecord> index;
    IndexHeader indexHeader;
    loadIndexFile(index, indexHeader, existsRecords.first());
    if (!index.isEmpty())
        m_minMotionTime = index.first().start + indexHeader.startTime;
    if (existsRecords.size() > 1)
        loadIndexFile(index, indexHeader, existsRecords.last());
    if (!index.isEmpty())
        m_lastRecordedTime = m_maxMotionTime = index.last().start + indexHeader.startTime;
}

QString QnMotionArchive::getFilePrefix(const QDate& datetime)
{
    return QnMotionHelper::instance()->getMotionDir(datetime, m_resource->getUniqueId());
}

QString QnMotionArchive::getChannelPrefix()
{
    return m_channel == 0 ? QString() : QString::number(m_channel);
}

void QnMotionArchive::fillFileNames(qint64 datetimeMs, QFile* motionFile, QFile* indexFile)
{
    QDateTime datetime = QDateTime::fromMSecsSinceEpoch(datetimeMs);
    QString fileName = getFilePrefix(datetime.date());
    if (motionFile)
        motionFile->setFileName(fileName + QString("motion_detailed_data") + getChannelPrefix() + ".bin");
    if (indexFile)
        indexFile->setFileName(fileName + QString("motion_detailed_index") + getChannelPrefix() + ".bin");
    QnMutex m_fileAccessMutex;
    QDir dir;
    dir.mkpath(fileName);
}

QnTimePeriodList QnMotionArchive::matchPeriod(const QRegion& region, qint64 msStartTime, qint64 msEndTime, int detailLevel)
{
    if (minTime() != (qint64)AV_NOPTS_VALUE)
        msStartTime = qMax(minTime(), msStartTime);
    msEndTime = qMin(msEndTime, m_maxMotionTime);

    QnTimePeriodList rez;
    QFile motionFile, indexFile;
    quint8* buffer = (quint8*) qMallocAligned(MOTION_DATA_RECORD_SIZE * 1024, 32);
    simd128i mask[Qn::kMotionGridWidth * Qn::kMotionGridHeight / 128];
    int maskStart, maskEnd;

    NX_ASSERT(((unsigned long)mask)%16 == 0);

    QnMetaDataV1::createMask(region, (char*)mask, &maskStart, &maskEnd);
    bool isFirstStep = true;

    while (msStartTime < msEndTime)
    {
        qint64 minTime, maxTime;
        dateBounds(msStartTime, minTime, maxTime);

        QVector<IndexRecord> index;
        IndexHeader indexHeader;
        fillFileNames(msStartTime, &motionFile, 0);
        if (!motionFile.open(QFile::ReadOnly) || !loadIndexFile(index, indexHeader, msStartTime)) {
            msStartTime = maxTime + 1;
            continue;
        }


        QVector<IndexRecord>::iterator startItr = index.begin();
        QVector<IndexRecord>::iterator endItr = index.end();

        if (isFirstStep) {
            startItr = std::upper_bound(index.begin(), index.end(), msStartTime-indexHeader.startTime);
            if (startItr > index.begin())
                startItr--;
        }
        if (maxTime <= msEndTime)
            endItr = std::upper_bound(startItr, index.end(), msEndTime-indexHeader.startTime);
        int totalSteps = endItr - startItr;

        motionFile.seek((startItr-index.begin()) * MOTION_DATA_RECORD_SIZE);

        // math file (one month)
        QVector<IndexRecord>::const_iterator i = startItr;
        while (totalSteps > 0)
        {
            int readed = motionFile.read((char*) buffer, MOTION_DATA_RECORD_SIZE * qMin(totalSteps,1024));
            if (readed <= 0)
                break;
            quint8* dataEnd = buffer + readed;
            quint8* curData = buffer;
            while (i < endItr && curData < dataEnd)
            {
                if (QnMetaDataV1::matchImage((simd128i*) curData, mask, maskStart, maskEnd))
                {
                    qint64 fullStartTime = i->start + indexHeader.startTime;
                    if (fullStartTime > msEndTime) {
                        totalSteps = 0;
                        break;
                    }

                    if (rez.empty())
                        rez.push_back(QnTimePeriod(fullStartTime, i->duration));
                    else {
                        QnTimePeriod& last = *(rez.end()-1);
                        if (fullStartTime <= last.startTimeMs + last.durationMs + detailLevel)
                            last.durationMs = qMax(last.durationMs, i->duration + fullStartTime - last.startTimeMs);
                        else
                            rez.push_back(QnTimePeriod(fullStartTime, i->duration));
                    }
                }
                curData += MOTION_DATA_RECORD_SIZE;
                ++i;
            }
            totalSteps -= readed/MOTION_DATA_RECORD_SIZE;
        }
        msStartTime = maxTime + 1;
        isFirstStep = false;
    }

    qFreeAligned(buffer);
    return rez;
}

bool QnMotionArchive::loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, const QDateTime& time)
{
    return loadIndexFile(index, indexHeader, time.toMSecsSinceEpoch());
}

bool QnMotionArchive::loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, const QDate& time)
{
    return loadIndexFile(index, indexHeader, QDateTime(time).toMSecsSinceEpoch());
}

bool QnMotionArchive::loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, qint64 msTime)
{
    QFile indexFile;
    fillFileNames(msTime, 0, &indexFile);
    if (!indexFile.open(QFile::ReadOnly))
        return false;
    return loadIndexFile(index, indexHeader, indexFile);
}

bool QnMotionArchive::loadIndexFile(QVector<IndexRecord>& index, IndexHeader& indexHeader, QFile& indexFile)
{
    static const size_t READ_BUF_SIZE = 128*1024;

    index.clear();
    indexFile.seek(0);
    indexFile.read((char*) &indexHeader, MOTION_INDEX_HEADER_SIZE);

    std::vector<quint8> tmpBuffer(READ_BUF_SIZE);
    while(1)
    {
        int readed = indexFile.read((char*) &tmpBuffer[0], READ_BUF_SIZE);
        if (readed < 1)
            break;
        int records = readed / MOTION_INDEX_RECORD_SIZE;
        int oldVectorSize = index.size();
        index.resize(oldVectorSize + records);
        memcpy(&index[oldVectorSize], &tmpBuffer[0], records * MOTION_INDEX_RECORD_SIZE);
    }

    return true;
}

void QnMotionArchive::dateBounds(qint64 datetimeMs, qint64& minDate, qint64& maxDate)
{
    QDateTime datetime = QDateTime::fromMSecsSinceEpoch(datetimeMs);
    QDateTime monthStart(datetime.date().addDays(1-datetime.date().day()));
    minDate = monthStart.toMSecsSinceEpoch();

    QDateTime nextMonth(monthStart.addMonths(1));
    maxDate = nextMonth.toMSecsSinceEpoch()-1;
}

int QnMotionArchive::getSizeForTime(qint64 timeMs, bool reloadIndex)
{
    if (reloadIndex)
        loadIndexFile(m_index, m_indexHeader, m_detailedIndexFile);
    QVector<IndexRecord>::iterator indexIterator = std::lower_bound(m_index.begin(), m_index.end(), timeMs - m_indexHeader.startTime);
    return indexIterator - m_index.begin();
}

bool QnMotionArchive::saveToArchiveInternal(QnConstMetaDataV1Ptr data)
{
    qint64 timestamp = data->timestamp/1000;
    if (timestamp > m_lastDateForCurrentFile || timestamp < m_firstTime)
    {
        // go to new file

        m_middleRecordNum = -1;
        m_index.clear();
        dateBounds(timestamp, m_firstTime, m_lastDateForCurrentFile);

        //QString fileName = getFilePrefix(datetime);
        m_detailedMotionFile.close();
        m_detailedIndexFile.close();
        fillFileNames(timestamp, &m_detailedMotionFile, &m_detailedIndexFile);
        if (!m_detailedMotionFile.open(QFile::ReadWrite))
            return false;

        if (!m_detailedIndexFile.open(QFile::ReadWrite))
            return false;

        // truncate biggest file. So, it is error checking
        int indexRecords = qMax((m_detailedIndexFile.size() - MOTION_INDEX_HEADER_SIZE) / MOTION_INDEX_RECORD_SIZE, 0ll);
        int dataRecords  = m_detailedMotionFile.size() / MOTION_DATA_RECORD_SIZE;
        if (indexRecords > dataRecords) {
            QnMutexLocker lock( &m_fileAccessMutex );
            if (!m_detailedIndexFile.resize(dataRecords*MOTION_INDEX_RECORD_SIZE + MOTION_INDEX_HEADER_SIZE))
                return false;
        }
        else if ((indexRecords < dataRecords)) {
            QnMutexLocker lock( &m_fileAccessMutex );
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
        else {
            loadIndexFile(m_index, m_indexHeader, m_detailedIndexFile);
            m_firstTime = m_indexHeader.startTime;
            if (m_index.size() > 0) {
                m_lastRecordedTime = m_maxMotionTime = m_index.last().start + m_indexHeader.startTime;
            }
        }
        m_detailedMotionFile.seek(m_detailedMotionFile.size());
        m_detailedIndexFile.seek(m_detailedIndexFile.size());
    }

    if (timestamp < m_lastRecordedTime)
    {
        // go to the file middle
        m_middleRecordNum = getSizeForTime(timestamp, true);
        m_detailedIndexFile.seek(m_middleRecordNum*MOTION_INDEX_RECORD_SIZE + MOTION_INDEX_HEADER_SIZE);
        m_detailedMotionFile.seek(m_middleRecordNum*MOTION_DATA_RECORD_SIZE);
        m_minMotionTime = qMin(m_minMotionTime, timestamp);
    }
    else if (m_middleRecordNum >= 0 && m_middleRecordNum < m_index.size())
    {
        qint64 timeInIndex = m_index[m_middleRecordNum].start + m_indexHeader.startTime;
        if (timestamp > timeInIndex) {
            m_middleRecordNum = getSizeForTime(timestamp, false);
            m_detailedIndexFile.seek(m_middleRecordNum*MOTION_INDEX_RECORD_SIZE + MOTION_INDEX_HEADER_SIZE);
            m_detailedMotionFile.seek(m_middleRecordNum*MOTION_DATA_RECORD_SIZE);
        }
    }

    quint32 relTime = quint32(timestamp - m_firstTime);
    quint32 duration = int((data->timestamp+data->m_duration)/1000 - timestamp);
    if (m_detailedIndexFile.write((const char*) &relTime, 4) != 4)
    {
        qWarning() << "Failed to write index file for camera" << m_resource->getUniqueId();
    }
    if (m_detailedIndexFile.write((const char*) &duration, 4) != 4)
    {
        qWarning() << "Failed to write index file for camera" << m_resource->getUniqueId();
    }

    if (m_detailedMotionFile.write(data->data(), data->dataSize())
            != static_cast<qint64>(data->dataSize()))
    {
        qWarning() << "Failed to write index file for camera" << m_resource->getUniqueId();
    }

    m_detailedIndexFile.flush();
    m_detailedMotionFile.flush();

    m_lastRecordedTime = timestamp;
    m_maxMotionTime = qMax(m_maxMotionTime, timestamp);
    if (m_minMotionTime == (qint64)AV_NOPTS_VALUE)
        m_minMotionTime = m_maxMotionTime;
    m_middleRecordNum++;
    return true;
}

bool QnMotionArchive::saveToArchive(QnConstMetaDataV1Ptr data)
{
    bool rez = true;

    if (m_lastTimestamp == (qint64)AV_NOPTS_VALUE) {
        m_lastDetailedData->m_duration = 0;
        m_lastDetailedData->timestamp = data->timestamp;
    }
    else {
        if(data->timestamp >= m_lastTimestamp && data->timestamp - m_lastTimestamp <= MAX_FRAME_DURATION_MS*1000ll) {
            m_lastDetailedData->m_duration = data->timestamp - m_lastDetailedData->timestamp;
        }
        else {
            ; // close motion period at previous packet
        }
    }

    if (data->timestamp >= m_lastDetailedData->timestamp && data->timestamp - m_lastDetailedData->timestamp < DETAILED_AGGREGATE_INTERVAL*1000000ll)
    {
        //qDebug() << "addMotion=" << QDateTime::fromMSecsSinceEpoch(data->timestamp/1000).toString("hh.mm.ss.zzz")
        //    << "to" << QDateTime::fromMSecsSinceEpoch(m_lastDetailedData->timestamp).toString("hh.mm.ss.zzz");

        // aggregate data
        m_lastDetailedData->addMotion(data);
    }
    else {
        // save to disk
        if (!m_lastDetailedData->isEmpty())
            rez = saveToArchiveInternal(m_lastDetailedData);
        m_lastDetailedData = QnMetaDataV1Ptr(data->clone());
        m_lastDetailedData->m_duration = 0;
        //qDebug() << "start new Motion" << QDateTime::fromMSecsSinceEpoch(data->timestamp/1000).toString("hh.mm.ss.zzz");
    }
    m_lastTimestamp = data->timestamp;
    return rez;
}

QnMotionArchiveConnectionPtr QnMotionArchive::createConnection()
{
    return QnMotionArchiveConnectionPtr(new QnMotionArchiveConnection(this));
}

qint64 QnMotionArchive::minTime() const
{
    return m_minMotionTime;
}

qint64 QnMotionArchive::maxTime() const
{
    return m_maxMotionTime;
}

int QnMotionArchive::getChannel() const
{
    return m_channel;
}
