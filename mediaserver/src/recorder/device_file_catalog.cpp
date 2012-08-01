#include <QDir>
#include "device_file_catalog.h"
#include "storage_manager.h"
#include "utils/common/util.h"
#include "recording/file_deletor.h"
#include "plugins/resources/archive/avi_files/avi_archive_delegate.h"
#include "recording/stream_recorder.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "motion/motion_helper.h"
#include <QDebug>
#include "recording_manager.h"
#include "serverutil.h"

QString DeviceFileCatalog::prefixForRole(QnResource::ConnectionRole role)
{
    return role == QnResource::Role_LiveVideo ? "hi_quality" : "low_quality";
}

QnResource::ConnectionRole DeviceFileCatalog::roleForPrefix(const QString& prefix)
{
    if (prefix == "hi_quality")
        return QnResource::Role_LiveVideo;
    else
        return QnResource::Role_SecondaryLiveVideo;
}



qint64 DeviceFileCatalog::Chunk::distanceToTime(qint64 timeMs) const
{
    if (timeMs >= startTimeMs) 
        return durationMs == -1 ? 0 : qMax(0ll, timeMs - (startTimeMs+durationMs));
    else 
        return startTimeMs - timeMs;
}

qint64 DeviceFileCatalog::Chunk::endTimeMs() const
{
    if (durationMs == -1)
        return DATETIME_NOW;
    else
        return startTimeMs + durationMs;
}

bool DeviceFileCatalog::Chunk::containsTime(qint64 timeMs) const
{
    if (startTimeMs == -1)
        return false;
    else 
        return qBetween(timeMs, startTimeMs, endTimeMs());
}

void DeviceFileCatalog::Chunk::truncate(qint64 timeMs)
{
    durationMs = qMax(0ll, timeMs - startTimeMs);
}

DeviceFileCatalog::DeviceFileCatalog(const QString& macAddress, QnResource::ConnectionRole role):
    m_firstDeleteCount(0),
    m_macAddress(macAddress),
    m_mutex(QMutex::Recursive),
    m_duplicateName(false),
    m_role(role)
{
#ifdef _TEST_TWO_SERVERS
    QString devTitleFile = closeDirPath(getDataDirectory()) + QString("test/record_catalog/media/");
#else
    QString devTitleFile = closeDirPath(getDataDirectory()) + QString("record_catalog/media/");
#endif
    devTitleFile += prefixForRole(role) + "/";
    devTitleFile += m_macAddress + "/";
    devTitleFile += QString("title.csv");
    m_file.setFileName(devTitleFile);
    QDir dir;
    dir.mkpath(QFileInfo(devTitleFile).absolutePath());
    if (!m_file.open(QFile::ReadWrite))
    {
        cl_log.log("Can't create title file ", devTitleFile, cl_logERROR);
        return;
    }

    if (m_file.size() == 0) 
    {
        QTextStream str(&m_file);
        str << "start; storage; index; duration\n"; // write CSV header
        str.flush();
    }
    else {
        deserializeTitleFile();
    }
}

bool DeviceFileCatalog::lastFileDuplicateName() const
{
    return m_duplicateName;
}

bool DeviceFileCatalog::fileExists(const Chunk& chunk)
{
    QnStorageResourcePtr storage = qnStorageMan->storageRoot(chunk.storageIndex);
    if (!storage)
        return false;
    QString prefix = closeDirPath(storage->getUrl()) + prefixForRole(m_role) + QString('/') + m_macAddress + QString('/');

   

    QDateTime fileDate = QDateTime::fromMSecsSinceEpoch(chunk.startTimeMs);
    int currentParts[4];
    currentParts[0] = fileDate.date().year();
    currentParts[1] = fileDate.date().month();
    currentParts[2] = fileDate.date().day();
    currentParts[3] = fileDate.time().hour();
    //QDir dir;
    bool sameDir = true;
    for (int i = 0; i < 4; ++i)
    {
        if (m_prevParts[i].first == currentParts[i])
        {
            // optimization. Check file existing without IO operation
            if (!m_prevParts[i].second)
                return false;
        }
        else 
        {
            sameDir = false;
            // new folder. check it
            for (int j = 0; j < i; ++j)
                prefix += strPadLeft(QString::number(currentParts[j]), 2, '0') + QString('/');

            bool exist = true;
            for (int j = i; j < 4; ++j)
            {
                prefix += strPadLeft(QString::number(currentParts[j]), 2, '0') + QString('/');
                if (exist)
                    exist &= storage->isDirExists(prefix);
                m_prevParts[j].first = currentParts[j];
                m_prevParts[j].second = exist;
            }
            break;
        }
    }
    if (!m_prevParts[3].second)
        return false;
    if (!sameDir) {
        m_existFileList = storage->getFileList(prefix);
    }
    QString fName = strPadLeft(QString::number(chunk.fileIndex), 3, '0') + QString(".mkv");

    bool found = false;
    foreach(const QFileInfo& info, m_existFileList)
    {
        if (info.fileName() == fName)
        {
            found = true;
            if (info.size() < 1024) 
            {
                // file is absent or empty media file
                storage->removeFile(info.absoluteFilePath()); // // delete broken file
                return false; 
            }
            storage->addWritedSpace(info.size());
            break;
        }
    }
    if (!found)
        return false;
    //if (!m_existFileList.contains(fName))
    //    return false;

    m_duplicateName = fName == m_prevFileName;
    m_prevFileName = fName;
    return true;
}

qint64 DeviceFileCatalog::recreateFile(const QString& fileName, qint64 startTimeMs, QnStorageResourcePtr storage)
{
    cl_log.log("recreate broken file ", fileName, cl_logWARNING);
    QnAviResourcePtr res(new QnAviResource(fileName));
    QnAviArchiveDelegate* avi = new QnAviArchiveDelegate();
    avi->setStorage(storage);
    if (!avi->open(res)) {
        delete avi;
        return 0;
    }
    QnArchiveStreamReader* reader = new QnArchiveStreamReader(res);
    reader->setArchiveDelegate(avi);
    QnStreamRecorder recorder(res);
    recorder.setFileName(fileName + ".new");
    recorder.setStorage(storage);
    recorder.setStartOffset(startTimeMs*1000);

    QnAbstractMediaDataPtr packet;
    while (packet = avi->getNextData())
    {
        packet->dataProvider = reader;
        recorder.processData(packet);
    }
    qint64 rez = recorder.duration()/1000;
    recorder.close();
    delete reader;

    if (storage->removeFile(fileName)) {
        storage->renameFile(fileName+".new.mkv", fileName);
    }
    return rez;
}

QList<QDate> DeviceFileCatalog::recordedMonthList()
{
    QVector<DeviceFileCatalog::Chunk>::iterator curItr = m_chunks.begin();
    QDate monthStart(1970, 1, 1);
    QList<QDate> rez;

    curItr = qLowerBound(curItr, m_chunks.end(), QDateTime(monthStart).toMSecsSinceEpoch());
    while (curItr != m_chunks.end())
    {
        monthStart = QDateTime::fromMSecsSinceEpoch(curItr->startTimeMs).date();
        monthStart = monthStart.addDays(1 - monthStart.day());
        rez << monthStart;
        monthStart = monthStart.addMonths(1);
        curItr = qLowerBound(curItr, m_chunks.end(), QDateTime(monthStart).toMSecsSinceEpoch());
    }
    return rez;
}

void DeviceFileCatalog::addChunk(const Chunk& chunk, qint64 lastStartTime)
{
    if (chunk.startTimeMs > lastStartTime) {
        m_chunks << chunk;
    }
    else {
        ChunkMap::iterator itr = qUpperBound(m_chunks.begin(), m_chunks.end(), chunk.startTimeMs);
        m_chunks.insert(itr, chunk);
    }
};

void DeviceFileCatalog::deserializeTitleFile()
{
    QMutexLocker lock(&m_mutex);
    bool needRewriteCatalog = false;
    QFile newFile(m_file.fileName() + QString(".tmp"));

    m_file.readLine(); // read header
    QByteArray line;
    qint64 lastStartTime = 0;
    do {
        line = m_file.readLine();
        QList<QByteArray> fields = line.split(';');
        if (fields.size() < 4) {
            continue;
        }
        int coeff = 1; // compabiliy with previous version (convert usec to ms)
        if (fields[0].length() >= 16)
            coeff = 1000;
        qint64 startTime = fields[0].toLongLong()/coeff;
        QString durationStr = fields[3].trimmed();
        int duration = fields[3].trimmed().toInt()/coeff;
        Chunk chunk(startTime, fields[1].toInt(), fields[2].toInt(), duration);

        QnStorageResourcePtr storage = qnStorageMan->storageRoot(chunk.storageIndex);
        if (fields[3].trimmed().isEmpty()) 
        {
            // duration unknown. server restart occured. Duration for chunk is unknown
            if (qnStorageMan->isStorageAvailable(chunk.storageIndex))
            {
                needRewriteCatalog = true;
                chunk.durationMs = recreateFile(fullFileName(chunk), chunk.startTimeMs, storage);
            }
            else {
                chunk.durationMs = 0;
            }
        }

        if (!qnStorageMan->isStorageAvailable(chunk.storageIndex)) 
        {
            ;
            // Skip chunks for unavaileble storage
             //addChunk(chunk, lastStartTime);
        }
        else if (fileExists(chunk)) 
        {
            if (chunk.durationMs > QnRecordingManager::RECORDING_CHUNK_LEN*1000 * 2 || chunk.durationMs < 1)
            {
                const QString fileName = fullFileName(chunk);

                qWarning() << "File " << fileName << "has invalid duration " << chunk.durationMs/1000.0 << "s and corrupted. Delete file from catalog";
                storage->removeFile(fileName);
                needRewriteCatalog = true;
                continue;
            }

            if (lastFileDuplicateName()) {
                if (m_chunks.isEmpty())
                    m_chunks << chunk;
                else
                    m_chunks.last() = chunk;
                needRewriteCatalog = true;
            }       
            else 
            {
                addChunk(chunk, lastStartTime);
            }
        }
        else {
            needRewriteCatalog = true;
        }
        lastStartTime = startTime;

    } while (!line.isEmpty());
    newFile.close();
    if (needRewriteCatalog && newFile.open(QFile::WriteOnly))
    {
        QTextStream str(&newFile);
        str << "start; storage; index; duration\n"; // write CSV header

        foreach(Chunk chunk, m_chunks)
            str << chunk.startTimeMs  << ';' << chunk.storageIndex << ';' << chunk.fileIndex << ';' << chunk.durationMs << '\n';
        str.flush();

        m_file.close();
        newFile.close();

        QnMotionHelper::instance()->deleteUnusedFiles(recordedMonthList(), m_macAddress);

        if (m_file.remove())
            newFile.rename(m_file.fileName());
        m_file.open(QFile::ReadWrite);
        m_file.seek(m_file.size());
    }
}

void DeviceFileCatalog::addRecord(const Chunk& chunk)
{
    Q_ASSERT(chunk.durationMs < 1000 * 1000);
    QMutexLocker lock(&m_mutex);

    ChunkMap::iterator itr = qUpperBound(m_chunks.begin(), m_chunks.end(), chunk.startTimeMs);
    m_chunks.insert(itr, chunk);
    QTextStream str(&m_file);

    str << chunk.startTimeMs << ';' << chunk.storageIndex << ';' << chunk.fileIndex << ';';
    if (chunk.durationMs >= 0)
        str << chunk.durationMs  << '\n';
    str.flush();
}

void DeviceFileCatalog::updateDuration(int durationMs)
{
    Q_ASSERT(durationMs < 1000 * 1000);
    QMutexLocker lock(&m_mutex);
    m_chunks.last().durationMs = durationMs;
    QTextStream str(&m_file);
    str << durationMs << '\n';
    str.flush();
}

void DeviceFileCatalog::deleteRecordsBeforeTime(qint64 timeMs)
{
    QMutexLocker lock(&m_mutex);
    while (!m_chunks.isEmpty() && m_firstDeleteCount < m_chunks.size() && m_chunks[m_firstDeleteCount].startTimeMs < timeMs)
        deleteFirstRecord();
}


void DeviceFileCatalog::deleteRecordsBefore(int idx)
{
    int count = idx - m_firstDeleteCount; // m_firstDeleteCount may be changed during delete
    for (int i = 0; i < count; ++i)
        deleteFirstRecord();
}

void DeviceFileCatalog::clear()
{
    while(m_firstDeleteCount < m_chunks.size())
        deleteFirstRecord();
}

void DeviceFileCatalog::deleteRecordsByStorage(int storageIndex, qint64 timeMs)
{
    QMutexLocker lock(&m_mutex);

    if (m_firstDeleteCount > 0) {
        emit firstDataRemoved(m_firstDeleteCount);
        m_chunks.erase(m_chunks.begin(), m_chunks.begin() + m_firstDeleteCount);
        m_firstDeleteCount = 0;
    }
    for (int i = 0; i < m_chunks.size();)
    {
        if (m_chunks[i].storageIndex == storageIndex)
        {
            if (m_chunks[i].startTimeMs < timeMs)
                m_chunks.remove(i);
            else
                break;

        }
        else {
            ++i;
        }
    }

}


bool DeviceFileCatalog::deleteFirstRecord()
{
    QMutexLocker lock(&m_mutex);
    if (m_chunks.isEmpty())
        return false;

    static const int DELETE_COEFF = 1000;

    if (m_firstDeleteCount < m_chunks.size()) 
    {
        QnStorageResourcePtr storage = qnStorageMan->storageRoot(m_chunks[m_firstDeleteCount].storageIndex);
        QString delFileName = fullFileName(m_chunks[m_firstDeleteCount]);

        storage->addWritedSpace(-storage->getFileSize(delFileName));
        storage->removeFile(delFileName);
        bool isLastChunkOfMonth = true;
        QDate curDate = QDateTime::fromMSecsSinceEpoch(m_chunks[m_firstDeleteCount].startTimeMs).date();
        if (m_firstDeleteCount < m_chunks.size()-1)
        {
            QDate nextDate = QDateTime::fromMSecsSinceEpoch(m_chunks[m_firstDeleteCount+1].startTimeMs).date();
            isLastChunkOfMonth = curDate.year() != nextDate.year() || curDate.month() != nextDate.month();
        }
        if (isLastChunkOfMonth) {
            QString motionDirName = QnMotionHelper::instance()->getMotionDir(curDate, m_macAddress);
            storage->removeDir(motionDirName);
        }
        m_firstDeleteCount++;
    }
    else {
        m_firstDeleteCount = 0;
        emit firstDataRemoved(m_chunks.size());
        m_chunks.clear();
    }
    if (m_firstDeleteCount == DELETE_COEFF) {
        emit firstDataRemoved(DELETE_COEFF);
        m_chunks.erase(m_chunks.begin(), m_chunks.begin() + DELETE_COEFF);
        m_firstDeleteCount = 0;
    }
    return true;
}

int DeviceFileCatalog::findFileIndex(qint64 startTimeMs, FindMethod method) const
{
/*
    QString msg;
    QTextStream str(&msg);
    str << " find chunk for time=" << QDateTime::fromMSecsSinceEpoch(startTime/1000).toString();
    str.flush();
    cl_log.log(msg, cl_logWARNING);
    str.flush();
*/
    QMutexLocker lock(&m_mutex);

    if (m_chunks.isEmpty())
        return -1;

    ChunkMap::const_iterator itr = qUpperBound(m_chunks.begin() + m_firstDeleteCount, m_chunks.end(), startTimeMs);
    if (itr > m_chunks.begin()+m_firstDeleteCount)
    {
        --itr;
         if (method == OnRecordHole_NextChunk && itr->startTimeMs + itr->durationMs <= startTimeMs && itr < m_chunks.end()-1)
         {
            ++itr;
         }
    }
    return itr - m_chunks.begin();
}

void DeviceFileCatalog::updateChunkDuration(Chunk& chunk)
{
    QMutexLocker lock(&m_mutex);
    ChunkMap::const_iterator itr = qLowerBound(m_chunks.begin() + m_firstDeleteCount, m_chunks.end(), chunk.startTimeMs);
    if (itr != m_chunks.end() && itr->startTimeMs == chunk.startTimeMs)
        chunk.durationMs = itr->durationMs;
}

QString DeviceFileCatalog::fullFileName(const Chunk& chunk) const
{
    QMutexLocker lock(&m_mutex);
    QnResourcePtr storage = qnStorageMan->storageRoot(chunk.storageIndex);
    if (!storage)
        return QString();
    return closeDirPath(storage->getUrl()) + 
                prefixForRole(m_role) + QString('/') +
                m_macAddress + QString('/') +
                QnStorageManager::dateTimeStr(chunk.startTimeMs) + 
                strPadLeft(QString::number(chunk.fileIndex), 3, '0') + 
                QString(".mkv");
}

qint64 DeviceFileCatalog::minTime() const
{
    QMutexLocker lock(&m_mutex);
    if (m_chunks.isEmpty() || m_firstDeleteCount >= m_chunks.size())
        return AV_NOPTS_VALUE;
    else
        return m_chunks[m_firstDeleteCount].startTimeMs;
}

qint64 DeviceFileCatalog::maxTime() const
{
    QMutexLocker lock(&m_mutex);
    if (m_chunks.isEmpty())
        return AV_NOPTS_VALUE;
    else
        return m_chunks.last().startTimeMs + qMax(0, m_chunks.last().durationMs);
}

bool DeviceFileCatalog::isLastChunk(qint64 startTimeMs) const
{
    QMutexLocker lock(&m_mutex);
    if (m_chunks.isEmpty())    
        return true;
    else
        return m_chunks.last().startTimeMs == startTimeMs;
}

DeviceFileCatalog::Chunk DeviceFileCatalog::chunkAt(int index) const
{
    QMutexLocker lock(&m_mutex);
    if (index < m_chunks.size() && index >= 0)
        return m_chunks.at(index);
    else
        return DeviceFileCatalog::Chunk();
}

qint64 DeviceFileCatalog::firstTime() const
{
    QMutexLocker lock(&m_mutex);
    if (m_firstDeleteCount >= m_chunks.size())
        return AV_NOPTS_VALUE;
    else
        return m_chunks[m_firstDeleteCount].startTimeMs;
}

QnTimePeriodList DeviceFileCatalog::getTimePeriods(qint64 startTime, qint64 endTime, qint64 detailLevel)
{
    //qDebug() << "find period from " << QDateTime::fromMSecsSinceEpoch(startTime).toString("hh:mm:ss.zzz") << "to" << QDateTime::fromMSecsSinceEpoch(endTime).toString("hh:mm:ss.zzz");

    QMutexLocker lock(&m_mutex);
    QnTimePeriodList result;
    ChunkMap::const_iterator itr = qLowerBound(m_chunks.begin() + m_firstDeleteCount, m_chunks.end(), startTime);
    if (itr == m_chunks.end())
        return result;

    int firstIndex = itr - m_chunks.begin();
    result << QnTimePeriod(m_chunks[firstIndex].startTimeMs, m_chunks[firstIndex].durationMs);

    for (int i = firstIndex+1; i < m_chunks.size() && m_chunks[i].startTimeMs < endTime; ++i)
    {
        QnTimePeriod& last = result.last();
        
        if (qAbs(last.startTimeMs + last.durationMs - m_chunks[i].startTimeMs) <= detailLevel && m_chunks[i].durationMs != -1)
            last.durationMs = m_chunks[i].startTimeMs - last.startTimeMs + m_chunks[i].durationMs;
        else {
            if (last.durationMs < detailLevel)
                result.pop_back();
            result << QnTimePeriod(m_chunks[i].startTimeMs, m_chunks[i].durationMs);
        }
    }
    //if (!result.isEmpty())
    //    qDebug() << "lastFoundPeriod start " << QDateTime::fromMSecsSinceEpoch(result.last().startTimeMs).toString("hh:mm:ss.zzz") << "dur=" << result.last().durationMs;

    return result;
}

bool operator < (qint64 first, const DeviceFileCatalog::Chunk& other)
{ 
    return first < other.startTimeMs;
}

bool operator < (const DeviceFileCatalog::Chunk& first, qint64 other)
{ 
    return first.startTimeMs < other;
}

bool operator < (const DeviceFileCatalog::Chunk& first, const DeviceFileCatalog::Chunk& other) 
{ 
    return first.startTimeMs < other.startTimeMs;
}

