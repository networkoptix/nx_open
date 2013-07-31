#include <QDir>
#include "device_file_catalog.h"
#include "storage_manager.h"
#include "utils/common/util.h"
#include <utils/fs/file.h>
#include "recorder/file_deletor.h"
#include "plugins/resources/archive/avi_files/avi_archive_delegate.h"
#include "recording/stream_recorder.h"
#include "plugins/resources/archive/avi_files/avi_resource.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "motion/motion_helper.h"
#include <QDebug>
#include "recording_manager.h"
#include "serverutil.h"

DeviceFileCatalog::RebuildMethod DeviceFileCatalog::m_rebuildArchive = DeviceFileCatalog::Rebuild_None;

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
    m_mutex(QMutex::Recursive),
    m_firstDeleteCount(0),
    m_macAddress(macAddress),
    //m_duplicateName(false),
    m_role(role),
    m_lastAddIndex(-1)
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
    dir.mkpath(QnFile::absolutePath(devTitleFile));
    if (!m_file.open(QFile::ReadWrite))
    {
        cl_log.log("Can't create title file ", devTitleFile, cl_logERROR);
        return;
    }

    if (m_file.size() == 0) 
    {
        QTextStream str(&m_file);
        str << "timezone; start; storage; index; duration\n"; // write CSV header
        str.flush();

    }
    else if (m_rebuildArchive == Rebuild_All || 
             (m_rebuildArchive == Rebuild_HQ && m_role == QnResource::Role_LiveVideo) ||
             (m_rebuildArchive == Rebuild_LQ && m_role == QnResource::Role_SecondaryLiveVideo)
            )
    {
        doRebuildArchive();
    }
    else {
        deserializeTitleFile();
    }
}

/*
bool DeviceFileCatalog::lastFileDuplicateName() const
{
    return m_duplicateName;
}
*/

QString getDirName(const QString& prefix, int currentParts[4], int i)
{
    QString result = prefix;
    for (int j = 0; j <= i; ++j)
        result += strPadLeft(QString::number(currentParts[j]), 2, '0') + QString('/');
    return result;
}

bool DeviceFileCatalog::fileExists(const Chunk& chunk, bool checkDirOnly)
{
    //fileSize = 0;
    QnStorageResourcePtr storage = qnStorageMan->storageRoot(chunk.storageIndex);
    if (!storage)
        return false;

    if (!storage->isCatalogAccessible())
        return true; // Can't check if file really exists

    QString prefix = closeDirPath(storage->getUrl()) + prefixForRole(m_role) + QString('/') + m_macAddress + QString('/');

   

    QDateTime fileDate = QDateTime::fromMSecsSinceEpoch(chunk.startTimeMs);
	if (chunk.timeZone != -1)
		fileDate = fileDate.toUTC().addSecs(chunk.timeZone*60);

    int currentParts[4];
    currentParts[0] = fileDate.date().year();
    currentParts[1] = fileDate.date().month();
    currentParts[2] = fileDate.date().day();
    currentParts[3] = fileDate.time().hour();

    bool sameDir = true;

    IOCacheMap::iterator itr = m_prevPartsMap->find(chunk.storageIndex);
    if (itr == m_prevPartsMap->end()) {
        itr = m_prevPartsMap->insert(chunk.storageIndex, IOCacheEntry());
    }
    CachedDirInfo& prevParts = itr.value().dirInfo;

    for (int i = 0; i < 4; ++i)
    {
        if (prevParts[i].first == currentParts[i])
        {
            // optimization. Check file existing without IO operation
            if (!prevParts[i].second)
                return false;
        }
        else 
        {
            sameDir = false;
            // new folder. check it

            for (int j = i; j < 4; ++j) {
                prevParts[j].first = currentParts[j];
                prevParts[j].second = true;
            }
            for (int j = 3; j >= i && !storage->isDirExists(getDirName(prefix, currentParts, j)); --j)
                prevParts[j].second = false;

            break;
        }
    }
    if (!prevParts[3].second)
        return false;

    // do not check files. just check dirs
    if (checkDirOnly)
        return true;

    if (!sameDir) {
        itr.value().entryList = storage->getFileList(getDirName(prefix, currentParts, 3));
    }
    QString fName = strPadLeft(QString::number(chunk.fileIndex), 3, '0') + QString(".mkv");

    bool found = false;
    foreach(const QFileInfo& info, itr.value().entryList)
    {
        if (info.fileName() == fName)
        {
            found = true;
            qint64 fileSize = info.size();
            if (fileSize < 1024) 
            {
                // file is absent or empty media file
                storage->removeFile(info.absoluteFilePath()); // // delete broken file
                return false; 
            }
            break;
        }
    }
    //m_prevFileNames[chunk.storageIndex] = fName;
    return found;
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

    //qint64 oldSize = storage->getFileSize(fileName);
    //qint64 newSize = storage->getFileSize(fileName + ".new.mkv");

    if (storage->removeFile(fileName)) {
        storage->renameFile(fileName+".new.mkv", fileName);
        //storage->addWritedSpace(newSize - oldSize);
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

bool DeviceFileCatalog::addChunk(const Chunk& chunk)
{
    if (!m_chunks.isEmpty() && chunk.startTimeMs > m_chunks.last().startTimeMs) {
        m_chunks << chunk;
    }
    else {
        ChunkMap::iterator itr = qUpperBound(m_chunks.begin()+m_firstDeleteCount, m_chunks.end(), chunk.startTimeMs);
        m_chunks.insert(itr, chunk);
    }
    if (m_chunks.size() > 1 && m_chunks.last().startTimeMs == m_chunks[m_chunks.size()-2].endTimeMs() + 1)
    {
        // update catalog to fix bug from previous version
        m_chunks[m_chunks.size()-2].durationMs = m_chunks.last().startTimeMs - m_chunks[m_chunks.size()-2].startTimeMs;
        return true;
    }
    return false;
}

DeviceFileCatalog::Chunk DeviceFileCatalog::chunkFromFile(QnStorageResourcePtr storage, const QString& fileName)
{
    Chunk chunk;

    QnAviResourcePtr res(new QnAviResource(fileName));
    QnAviArchiveDelegate* avi = new QnAviArchiveDelegate();
    avi->setStorage(storage);
    avi->setFastStreamFind(true);
    if (avi->open(res) && avi->findStreams() && avi->endTime() != (qint64)AV_NOPTS_VALUE) {
        qint64 startTimeMs = avi->startTime()/1000;
        qint64 endTimeMs = avi->endTime()/1000;
        int fileIndex = QnFile::baseName(fileName).toInt();
        chunk = Chunk(startTimeMs, storage->getIndex(), fileIndex, endTimeMs - startTimeMs, currentTimeZone()/60);
    }
    delete avi;
    return chunk;
}

void DeviceFileCatalog::scanMediaFiles(const QString& folder, QnStorageResourcePtr storage, QMap<qint64, Chunk>& allChunks)
{
    QDir dir(folder);
    foreach(const QFileInfo& fi, dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name))
    {
        if (fi.isDir())
            scanMediaFiles(fi.absoluteFilePath(), storage, allChunks);
        else {
            Chunk chunk = chunkFromFile(storage, fi.absoluteFilePath());
            
            if (chunk.durationMs > 0) {
                QMap<qint64, Chunk>::iterator itr = allChunks.insert(chunk.startTimeMs, chunk);
                if (itr != allChunks.begin())
                {
                    Chunk& prevChunk = *(itr-1);
                    qint64 delta = chunk.startTimeMs - prevChunk.endTimeMs();
                    if (delta < MAX_FRAME_DURATION)
                        prevChunk.durationMs = chunk.startTimeMs - prevChunk.startTimeMs;
                }

                if (allChunks.size() % 100 == 0)
                    qWarning() << allChunks.size() << "media files processed...";

            }
        }

    }
}

void DeviceFileCatalog::readStorageData(QnStorageResourcePtr storage, QnResource::ConnectionRole role, QMap<qint64, Chunk>& allChunks)
{
    QString rootFolder = closeDirPath(storage->getUrl()) + prefixForRole(role) + QString('/') + m_macAddress;
    scanMediaFiles(rootFolder, storage, allChunks);
}

void DeviceFileCatalog::doRebuildArchive()
{
    QTime t;
    t.restart();
    qWarning() << "start rebuilding archive for camera " << m_macAddress << prefixForRole(m_role);

    QMap<qint64, Chunk> allChunks;
    foreach(QnStorageResourcePtr storage, qnStorageMan->getStorages())
        readStorageData(storage, m_role, allChunks);

    foreach(const Chunk& chunk, allChunks)
        m_chunks << chunk;

    rewriteCatalog();
    qWarning() << "rebuild archive for camera " << m_macAddress << prefixForRole(m_role) << "finished. time=" << t.elapsed() << "ms. processd files=" << m_chunks.size();
}

void DeviceFileCatalog::deserializeTitleFile()
{
    QTime t;
    t.restart();

    QMutexLocker lock(&m_mutex);
    bool needRewriteCatalog = false;

    int timeZoneExist = 0;
    QByteArray headerLine = m_file.readLine();
    if (headerLine.contains("timezone"))
        timeZoneExist = 1;
    QByteArray line;
    bool checkDirOnly = false;
    do {
        line = m_file.readLine();
        QList<QByteArray> fields = line.split(';');
        if (fields.size() < 4 + timeZoneExist) {
            continue;
        }
        int coeff = 1; // compabiliy with previous version (convert usec to ms)
        if (fields[0+timeZoneExist].length() >= 16)
            coeff = 1000;
        qint16 timeZone = timeZoneExist ? fields[0].toInt() : -1;
        qint64 startTime = fields[0+timeZoneExist].toLongLong()/coeff;
        QString durationStr = fields[3+timeZoneExist].trimmed();
        int duration = fields[3+timeZoneExist].trimmed().toInt()/coeff;
        Chunk chunk(startTime, fields[1+timeZoneExist].toInt(), fields[2+timeZoneExist].toInt(), duration, timeZone);

        QnStorageResourcePtr storage = qnStorageMan->storageRoot(chunk.storageIndex);
        if (fields[3+timeZoneExist].trimmed().isEmpty()) 
        {
            // duration unknown. server restart occured. Duration for chunk is unknown
            if (qnStorageMan->isStorageAvailable(chunk.storageIndex))
            {
                needRewriteCatalog = true;
                //chunk.durationMs = recreateFile(fullFileName(chunk), chunk.startTimeMs, storage);
                storage->removeFile(fullFileName(chunk));
                continue;
            }
            else {
                chunk.durationMs = 0;
            }
        }

        //qint64 chunkFileSize = 0;
        if (!qnStorageMan->isStorageAvailable(chunk.storageIndex)) 
        {
             needRewriteCatalog |= addChunk(chunk);
        }
        else if (fileExists(chunk, checkDirOnly))
        {
            //chunk.setFileSize(chunkFileSize);

            // optimization. Since we have got first file, check dirs only. It is required to check files at begin to determine archive start point
            checkDirOnly = true; 

            if (chunk.durationMs > QnRecordingManager::RECORDING_CHUNK_LEN*1000 * 2 || chunk.durationMs < 1)
            {
                const QString fileName = fullFileName(chunk);

                qWarning() << "File " << fileName << "has invalid duration " << chunk.durationMs/1000.0 << "s and corrupted. Delete file from catalog";
                storage->removeFile(fileName);
                needRewriteCatalog = true;
                continue;
            }
			//storage->addWritedSpace(chunkFileSize);

            /*
            if (lastFileDuplicateName()) {
                if (m_chunks.isEmpty())
                    m_chunks << chunk;
                else
                    m_chunks.last() = chunk;
                needRewriteCatalog = true;
            }       
            else 
            */
            {
                needRewriteCatalog |= addChunk(chunk);
            }
        }
        else {
            needRewriteCatalog = true;
        }

    } while (!line.isEmpty());
    
    if (!timeZoneExist)
        needRewriteCatalog = true; // update catalog to new version

    if (needRewriteCatalog)
        rewriteCatalog();

    qWarning() << QString("Check archive for camera %1 for role %2 time: %3 ms").arg(m_macAddress).arg(m_role).arg(t.elapsed());
}

void DeviceFileCatalog::rewriteCatalog()
{
    QFile newFile(m_file.fileName() + QString(".tmp"));

    if (newFile.open(QFile::WriteOnly))
    {
        QTextStream str(&newFile);
        str << "timezone; start; storage; index; duration\n"; // write CSV header

        foreach(Chunk chunk, m_chunks)
            str << chunk.timeZone << ';' << chunk.startTimeMs  << ';' << chunk.storageIndex << ';' << chunk.fileIndex << ';' << chunk.durationMs << '\n';
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

    {
        QMutexLocker lock(&m_mutex);
        ChunkMap::iterator itr = qUpperBound(m_chunks.begin()+m_firstDeleteCount, m_chunks.end(), chunk.startTimeMs);
        itr = m_chunks.insert(itr, chunk);
        m_lastAddIndex = itr - m_chunks.begin();
        //if (m_lastAddIndex < m_chunks.size()-1)
        //    itr->durationMs = 0; // if insert to the archive middle, reset 'continue recording' mark
    }

    QMutexLocker lock(&m_IOMutex);
    QTextStream str(&m_file);
    str << chunk.timeZone << ';' << chunk.startTimeMs << ';' << chunk.storageIndex << ';' << chunk.fileIndex << ';';
    if (chunk.durationMs >= 0)
        str << chunk.durationMs  << '\n';
    str.flush();
}

void DeviceFileCatalog::updateDuration(int durationMs, qint64 fileSize)
{
    Q_ASSERT(durationMs < 1000 * 1000);
    {
        QMutexLocker lock(&m_mutex);
        //m_chunks.last().durationMs = durationMs;
        if (m_lastAddIndex >= 0) {
            m_chunks[m_lastAddIndex].durationMs = durationMs;
            m_chunks[m_lastAddIndex].setFileSize(fileSize);
        }
    }

    QMutexLocker lock(&m_IOMutex);
    QTextStream str(&m_file);
    str << durationMs << '\n';
    str.flush();
}

qint64 DeviceFileCatalog::deleteRecordsBefore(int idx)
{
    int count = idx - m_firstDeleteCount; // m_firstDeleteCount may be changed during delete
    qint64 rez = 0;
    for (int i = 0; i < count; ++i)
        rez += deleteFirstRecord();
    return rez;
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
        m_lastAddIndex -= m_firstDeleteCount;
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


qint64 DeviceFileCatalog::deleteFirstRecord()
{
	QnStorageResourcePtr storage;
	QString delFileName;
	QString motionDirName;
    qint64 deletedSize = 0;
	{
		QMutexLocker lock(&m_mutex);
		if (m_chunks.isEmpty())
			return -1;

		static const int DELETE_COEFF = 1000;

		if (m_firstDeleteCount < m_chunks.size()) 
		{
			storage = qnStorageMan->storageRoot(m_chunks[m_firstDeleteCount].storageIndex);
			delFileName = fullFileName(m_chunks[m_firstDeleteCount]);
            deletedSize = m_chunks[m_firstDeleteCount].getFileSize();

			QDate curDate = QDateTime::fromMSecsSinceEpoch(m_chunks[m_firstDeleteCount].startTimeMs).date();

			if (m_firstDeleteCount < m_chunks.size()-1)
			{
				QDate nextDate = QDateTime::fromMSecsSinceEpoch(m_chunks[m_firstDeleteCount+1].startTimeMs).date();
				if (curDate.year() != nextDate.year() || curDate.month() != nextDate.month())
					motionDirName = QnMotionHelper::instance()->getMotionDir(curDate, m_macAddress);
			}
			m_firstDeleteCount++;
		}
		else {
			m_firstDeleteCount = 0;
			emit firstDataRemoved(m_chunks.size());
			m_chunks.clear();
			m_lastAddIndex = -1;
		}
		if (m_firstDeleteCount == DELETE_COEFF) {
			emit firstDataRemoved(DELETE_COEFF);
			m_chunks.erase(m_chunks.begin(), m_chunks.begin() + DELETE_COEFF);
			m_lastAddIndex -= DELETE_COEFF;
			m_firstDeleteCount = 0;
		}
	}

    if (storage) {
	    if (!delFileName.isEmpty())
	    {
            if (deletedSize == 0)
                deletedSize = storage->getFileSize(delFileName); // obtain file size from a disk
		    //storage->addWritedSpace(-deletedSize);
		    storage->removeFile(delFileName);
	    }
	    if (!motionDirName.isEmpty())
		    storage->removeDir(motionDirName);
    }

    return deletedSize;
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
    QnResourcePtr storage = qnStorageMan->storageRoot(chunk.storageIndex);
    if (!storage)
        return QString();
    return closeDirPath(storage->getUrl()) + 
                prefixForRole(m_role) + QString('/') +
                m_macAddress + QString('/') +
                QnStorageManager::dateTimeStr(chunk.startTimeMs, chunk.timeZone) + 
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
    else if (m_lastAddIndex >= 0 && m_chunks[m_lastAddIndex].durationMs == -1)
        return DATETIME_NOW;
    else if (m_chunks.last().durationMs == -1)
        return DATETIME_NOW;
    else
        return m_chunks.last().startTimeMs + qMax(0, m_chunks.last().durationMs);
}

bool DeviceFileCatalog::containTime(qint64 timeMs, qint64 eps) const
{
    if (m_chunks.isEmpty())
        return false;

    ChunkMap::const_iterator itr = qUpperBound(m_chunks.begin() + m_firstDeleteCount, m_chunks.end(), timeMs);
    if (itr != m_chunks.end() && itr->startTimeMs - timeMs <= eps)
        return true;
    if (itr > m_chunks.begin()+m_firstDeleteCount)
        --itr;
    return itr->distanceToTime(timeMs) <= eps;
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

void DeviceFileCatalog::setRebuildArchive(RebuildMethod value)
{
    m_rebuildArchive = value;
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

