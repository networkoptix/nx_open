#include "device_file_catalog.h"
#include "storage_manager.h"
#include "utils/common/util.h"
#include "file_deletor.h"
#include "plugins/resources/archive/avi_files/avi_archive_delegate.h"
#include "stream_recorder.h"
#include "plugins/resources/archive/avi_files/avi_device.h"
#include "plugins/resources/archive/archive_stream_reader.h"

bool operator < (const QnTimePeriod& first, const QnTimePeriod& other) 
{
    return first.startTimeUSec < other.startTimeUSec; 
}

bool operator < (qint64 first, const QnTimePeriod& other) 
{ 
    return first < other.startTimeUSec; 
}

bool operator < (const QnTimePeriod& other, qint64 first) 
{ 
    return other.startTimeUSec < first; 
}

DeviceFileCatalog::DeviceFileCatalog(const QString& macAddress):
    m_firstDeleteCount(0),
    m_macAddress(macAddress),
    m_mutex(QMutex::Recursive),
    m_duplicateName(false)
{
    QString devTitleFile = closeDirPath(getDataDirectory()) + QString("record_catalog/media/") + m_macAddress + QString("/title.csv");
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
    QnResourcePtr storage = qnStorageMan->storageRoot(chunk.storageIndex);
    if (!storage)
        return false;
    QString prefix = closeDirPath(storage->getUrl()) + m_macAddress + QString('/');


    QDateTime fileDate = QDateTime::fromMSecsSinceEpoch(chunk.startTime/1000);
    int currentParts[4];
    currentParts[0] = fileDate.date().year();
    currentParts[1] = fileDate.date().month();
    currentParts[2] = fileDate.date().day();
    currentParts[3] = fileDate.time().hour();
    QDir dir;
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
                    exist &= dir.exists(prefix);
                m_prevParts[j].first = currentParts[j];
                m_prevParts[j].second = exist;
            }
            break;
        }
    }
    if (!m_prevParts[3].second)
        return false;
    if (!sameDir) {
        dir.cd(prefix);
        m_existFileList = dir.entryList(QDir::Files);
    }
    QString fName = strPadLeft(QString::number(chunk.fileIndex), 3, '0') + QString(".mkv");
    if (!m_existFileList.contains(fName))
        return false;

    m_duplicateName = fName == m_prevFileName;
    m_prevFileName = fName;
    return true;
}

qint64 DeviceFileCatalog::recreateFile(const QString& fileName, qint64 startTime)
{
    cl_log.log("recreate broken file ", fileName, cl_logWARNING);
    QnAviResourcePtr res(new QnAviResource(fileName));
    QnAviArchiveDelegate* avi = new QnAviArchiveDelegate();
    if (!avi->open(res)) {
        delete avi;
        return 0;
    }
    QnArchiveStreamReader* reader = new QnArchiveStreamReader(res);
    reader->setArchiveDelegate(avi);
    QnStreamRecorder recorder(res);
    recorder.setFileName(fileName + ".new");
    recorder.setStartOffset(startTime);

    QnAbstractMediaDataPtr packet;
    while (packet = avi->getNextData())
    {
        packet->dataProvider = reader;
        recorder.processData(packet);
    }
    qint64 rez = recorder.duration();
    recorder.close();
    delete reader;

    if (QFile::remove(fileName)) {
        QFile::rename(fileName+".new.mkv", fileName);
    }
    return rez;
}

void DeviceFileCatalog::deserializeTitleFile()
{
    QMutexLocker lock(&m_mutex);
    bool needRewriteFile = false;
    QFile newFile(m_file.fileName() + QString(".tmp"));

    m_file.readLine(); // read header
    QByteArray line;
    do {
        line = m_file.readLine();
        QList<QByteArray> fields = line.split(';');
        if (fields.size() < 3)
            continue;
        qint64 startTime = fields[0].toLongLong();
        int duration = fields[3].trimmed().toInt();
        Chunk chunk(startTime, fields[1].toInt(), fields[2].toInt(), duration);
        if (fields[3].trimmed().isEmpty()) 
        {
            // duration unknown. server restart occured. Duration for chunk is unknown
            needRewriteFile = true;
            chunk.duration = recreateFile(fullFileName(chunk), chunk.startTime);
        }
        if (fileExists(chunk)) 
        {
            if (lastFileDuplicateName()) {
                m_chunks.last() = chunk;
                needRewriteFile = true;
            }       
            else {
                ChunkMap::iterator itr = qUpperBound(m_chunks.begin(), m_chunks.end(), startTime);
                m_chunks.insert(itr, chunk);
            }
        }
        else {
            needRewriteFile = true;
        }
    } while (!line.isEmpty());
    newFile.close();
    if (needRewriteFile && newFile.open(QFile::WriteOnly))
    {
        QTextStream str(&newFile);
        str << "start; storage; index; duration\n"; // write CSV header

        foreach(Chunk chunk, m_chunks)
            str << chunk.startTime  << ';' << chunk.storageIndex << ';' << chunk.fileIndex << ';' << chunk.duration << '\n';
        str.flush();

        m_file.close();
        newFile.close();
        if (m_file.remove())
            newFile.rename(m_file.fileName());
        m_file.open(QFile::ReadWrite);
        m_file.seek(m_file.size());
    }
}

void DeviceFileCatalog::addRecord(const Chunk& chunk)
{
    QMutexLocker lock(&m_mutex);

    ChunkMap::iterator itr = qUpperBound(m_chunks.begin(), m_chunks.end(), chunk.startTime);
    m_chunks.insert(itr, chunk);
    QTextStream str(&m_file);

    str << chunk.startTime << ';' << chunk.storageIndex << ';' << chunk.fileIndex << ';';
    if (chunk.duration >= 0)
        str << chunk.duration  << '\n';
    str.flush();
}

void DeviceFileCatalog::updateDuration(int duration)
{
    QMutexLocker lock(&m_mutex);
    m_chunks.last().duration = duration;
    QTextStream str(&m_file);
    str << duration  << '\n';
    str.flush();
}

bool DeviceFileCatalog::deleteFirstRecord()
{
    QMutexLocker lock(&m_mutex);
    if (m_chunks.isEmpty())
        return false;

    static const int DELETE_COEFF = 1000;

    if (m_firstDeleteCount < m_chunks.size()) 
    {
        QString delFileName = fullFileName(m_chunks[m_firstDeleteCount]);
        qnFileDeletor->deleteFile(delFileName);
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

int DeviceFileCatalog::findFileIndex(qint64 startTime, FindMethod method) const
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

    ChunkMap::const_iterator itr = qUpperBound(m_chunks.begin() + m_firstDeleteCount, m_chunks.end(), startTime);
    if (itr > m_chunks.begin())
    {
        --itr;
         if (method == OnRecordHole_NextChunk && itr->startTime + itr->duration < startTime && itr < m_chunks.end()-1)
             ++itr;
    }
    return itr - m_chunks.begin();
}

QString DeviceFileCatalog::fullFileName(const Chunk& chunk) const
{
    QMutexLocker lock(&m_mutex);
    QnResourcePtr storage = qnStorageMan->storageRoot(chunk.storageIndex);
    if (!storage)
        return QString();
    return closeDirPath(storage->getUrl()) + 
                m_macAddress + QString('/') +
                QnStorageManager::dateTimeStr(chunk.startTime) + 
                strPadLeft(QString::number(chunk.fileIndex), 3, '0') + 
                QString(".mkv");
}

qint64 DeviceFileCatalog::minTime() const
{
    QMutexLocker lock(&m_mutex);
    if (m_chunks.isEmpty() || m_firstDeleteCount >= m_chunks.size())
        return AV_NOPTS_VALUE;
    else
        return m_chunks[m_firstDeleteCount].startTime;
}

qint64 DeviceFileCatalog::maxTime() const
{
    QMutexLocker lock(&m_mutex);
    if (m_chunks.isEmpty())
        return AV_NOPTS_VALUE;
    else
        return m_chunks.last().startTime + qMax(0, m_chunks.last().duration);
}

DeviceFileCatalog::Chunk DeviceFileCatalog::chunkAt(int index) const
{
    QMutexLocker lock(&m_mutex);
    if (index < m_chunks.size())
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
        return m_chunks[m_firstDeleteCount].startTime;
}

QnTimePeriodList DeviceFileCatalog::getTimePeriods(qint64 startTime, qint64 endTime, qint64 detailLevel)
{
    QMutexLocker lock(&m_mutex);
    QnTimePeriodList result;
    ChunkMap::const_iterator itr = qLowerBound(m_chunks.begin() + m_firstDeleteCount, m_chunks.end(), startTime);
    if (itr == m_chunks.end())
        return result;

    int firstIndex = itr - m_chunks.begin();
    result << QnTimePeriod(m_chunks[firstIndex].startTime, m_chunks[firstIndex].duration);

    for (int i = firstIndex+1; i < m_chunks.size() && m_chunks[i].startTime < endTime; ++i)
    {
        QnTimePeriod& last = result.last();
        qint64 ggC = m_chunks[i].startTime;
        if (qAbs(last.startTimeUSec + last.durationUSec - m_chunks[i].startTime) <= detailLevel && m_chunks[i].duration != -1)
            last.durationUSec = m_chunks[i].startTime - last.startTimeUSec + m_chunks[i].duration;
        else {
            if (last.durationUSec < detailLevel)
                result.pop_back();
            result << QnTimePeriod(m_chunks[i].startTime, m_chunks[i].duration);
        }
    }
    return result;
}

bool operator < (qint64 first, const DeviceFileCatalog::Chunk& other)
{ 
    return first < other.startTime; 
}

bool operator < (const DeviceFileCatalog::Chunk& first, qint64 other)
{ 
    return first.startTime < other; 
}

bool operator < (const DeviceFileCatalog::Chunk& first, const DeviceFileCatalog::Chunk& other) 
{ 
    return first.startTime < other.startTime; 
}

QnTimePeriodList QnTimePeriod::mergeTimePeriods(QVector<QnTimePeriodList> periods)
{
    QnTimePeriodList result;
    int minIndex = 0;
    while (minIndex != -1)
    {
        qint64 minStartTime = 0x7fffffffffffffffll;
        minIndex = -1;
        for (int i = 0; i < periods.size(); ++i) {
            if (!periods[i].isEmpty() && periods[i][0].startTimeUSec < minStartTime) {
                minIndex = i;
                minStartTime = periods[i][0].startTimeUSec;
            }
        }

        if (minIndex >= 0)
        {
            // add chunk to merged data
            if (result.isEmpty()) {
                result << periods[minIndex][0];
            }
            else {
                QnTimePeriod& last = result.last();
                if (last.startTimeUSec <= minStartTime && last.startTimeUSec+last.durationUSec > minStartTime)
                    last.durationUSec = qMax(last.durationUSec, minStartTime + periods[minIndex][0].durationUSec - last.startTimeUSec);
                else {
                    result << periods[minIndex][0];
                    if (periods[minIndex][0].durationUSec == -1)
                        break;
                }
            } 
            periods[minIndex].pop_front();
        }
    }
    return result;
}