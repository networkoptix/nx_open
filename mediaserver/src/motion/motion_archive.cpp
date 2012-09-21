#include "motion_archive.h"
#include <QDateTime>
#include <QDir>
#include "utils/common/util.h"
#include "motion_helper.h"

#ifdef Q_OS_MAC
#include <smmintrin.h>
#endif

static const char version = 1;
static const quint16 DETAILED_AGGREGATE_INTERVAL = 3; // at seconds
static const quint16 COARSE_AGGREGATE_INTERVAL = 3600; // at seconds

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

QnMetaDataV1Ptr QnMotionArchiveConnection::getMotionData(qint64 timeUsec)
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
            m_lastTimeMs = AV_NOPTS_VALUE;
            m_motionLoadedStart = m_motionLoadedEnd = -1;
            if (!m_owner->loadIndexFile(m_index, m_indexHeader, timeMs))
            {
                m_maxDate = AV_NOPTS_VALUE;
                return QnMetaDataV1Ptr();
            }
            m_maxDate = qMin(m_maxDate, QDateTime::currentDateTime().toMSecsSinceEpoch());
        }
    }

    if (m_lastTimeMs != AV_NOPTS_VALUE && timeMs > m_lastTimeMs)
        m_indexItr = qUpperBound(m_indexItr, m_index.end(), timeMs - m_indexHeader.startTime);
    else 
        m_indexItr = qUpperBound(m_index.begin(), m_index.end(), timeMs - m_indexHeader.startTime);

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
    m_lastResult->data.clear();
    int motionIndex = indexOffset - m_motionLoadedStart;
    m_lastResult->data.write((const char*) m_motionBuffer + motionIndex*MOTION_DATA_RECORD_SIZE, MOTION_DATA_RECORD_SIZE);
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
    m_lastTimestamp(AV_NOPTS_VALUE)
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
    QList<QDate> existsRecords = QnMotionHelper::instance()->recordedMonth(m_resource->getPhysicalId());
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
        m_maxMotionTime = index.last().start + indexHeader.startTime;
}

QString QnMotionArchive::getFilePrefix(const QDate& datetime)
{
    return QnMotionHelper::instance()->getMotionDir(datetime, m_resource->getPhysicalId());
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
    QMutex m_fileAccessMutex;
    QDir dir;
    dir.mkpath(fileName);
}

QnTimePeriodList QnMotionArchive::mathPeriod(const QRegion& region, qint64 msStartTime, qint64 msEndTime, int detailLevel)
{
    if (minTime() != AV_NOPTS_VALUE)
        msStartTime = qMax(minTime(), msStartTime);
    msEndTime = qMin(msEndTime, m_maxMotionTime);

    QnTimePeriodList rez;
    QFile motionFile, indexFile;
    quint8* buffer = (quint8*) qMallocAligned(MOTION_DATA_RECORD_SIZE * 1024, 32);
    __m128i mask[MD_WIDTH * MD_HEIGHT / 128];
    int maskStart, maskEnd;

    Q_ASSERT(((unsigned long)mask)%16 == 0);

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
            startItr = qUpperBound(index.begin(), index.end(), msStartTime-indexHeader.startTime);
            if (startItr > index.begin())
                startItr--;
        }
        if (maxTime <= msEndTime)
            endItr = qUpperBound(startItr, index.end(), msEndTime-indexHeader.startTime);
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
                if (QnMetaDataV1::mathImage((__m128i*) curData, mask, maskStart, maskEnd))
                {
                    qint64 fullStartTime = i->start + minTime;
                    if (fullStartTime >= msEndTime) {
                        totalSteps = 0;
                        break;
                    }

                    if (!rez.isEmpty() && fullStartTime < rez.last().startTimeMs + rez.last().durationMs + (detailLevel-1))
                        rez.last().durationMs = qMax(rez.last().durationMs, i->duration + fullStartTime - rez.last().startTimeMs);
                    else
                        rez.push_back(QnTimePeriod(fullStartTime, i->duration));
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

    //qint64 indexSize = (indexFile.size()-MOTION_INDEX_HEADER_SIZE);
    //if (indexSize == 0)
    //    return false;
    //index.resize(indexSize/MOTION_INDEX_RECORD_SIZE);
    index.clear();
    indexFile.read((char*) &indexHeader, MOTION_INDEX_HEADER_SIZE);
    //indexFile.read((char*) &index[0], indexSize);

    quint8 tmpBuffer[1024*128];
    while(1)
    {
        int readed = indexFile.read((char*) tmpBuffer, sizeof(tmpBuffer));
        if (readed < 1)
            break;
        int records = readed / MOTION_INDEX_RECORD_SIZE;
        int oldVectorSize = index.size();
        index.resize(oldVectorSize + records);
        memcpy(&index[oldVectorSize], tmpBuffer, records * MOTION_INDEX_RECORD_SIZE);
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

bool QnMotionArchive::saveToArchiveInternal(QnMetaDataV1Ptr data)
{
    qint64 timestamp = data->timestamp/1000;
    if (timestamp > m_lastDateForCurrentFile)
    {

        dateBounds(timestamp, m_firstTime, m_lastDateForCurrentFile);

        //QString fileName = getFilePrefix(datetime);
        m_detailedMotionFile.close();
        m_detailedIndexFile.close();
        fillFileNames(timestamp, &m_detailedMotionFile, &m_detailedIndexFile);
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
    quint32 relTime = quint32(timestamp - m_firstTime);
    quint32 duration = int(data->m_duration/1000);
    if (m_detailedIndexFile.write((const char*) &relTime, 4) != 4)
    {
        qWarning() << "Failed to write index file for camera" << m_resource->getUniqueId();
    }
    if (m_detailedIndexFile.write((const char*) &duration, 4) != 4)
    {
        qWarning() << "Failed to write index file for camera" << m_resource->getUniqueId();
    }

    if (m_detailedMotionFile.write(data->data.constData(), data->data.capacity()) != data->data.capacity())
    {
        qWarning() << "Failed to write index file for camera" << m_resource->getUniqueId();
    }

    m_detailedIndexFile.flush();
    m_detailedMotionFile.flush();

    m_maxMotionTime = data->timestamp/1000;
    if (m_minMotionTime == AV_NOPTS_VALUE)
        m_minMotionTime = m_maxMotionTime;
    return true;
}

bool QnMotionArchive::saveToArchive(QnMetaDataV1Ptr data)
{
    bool rez = true;

    if (m_lastTimestamp == AV_NOPTS_VALUE) {
        m_lastDetailedData->m_duration = 0;
        m_lastDetailedData->timestamp = data->timestamp;
    }
    else {
        if(data->timestamp - m_lastTimestamp <= MAX_FRAME_DURATION*1000ll) {
            m_lastDetailedData->m_duration = data->timestamp - m_lastDetailedData->timestamp;
        }
        else {
            ; // close motion period at previous packet
        }
    }

    if (data->timestamp - m_lastDetailedData->timestamp < DETAILED_AGGREGATE_INTERVAL*1000000ll)
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
        m_lastDetailedData = data;
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
