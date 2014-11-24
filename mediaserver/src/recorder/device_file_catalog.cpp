#include "device_file_catalog.h"

#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>

#include <core/resource/resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource_management/resource_pool.h>

#include "storage_manager.h"
#include "utils/common/util.h"
#include <utils/fs/file.h>
#include <utils/common/log.h>
#include <utils/math/math.h>
#include "recorder/file_deletor.h"
#include "plugins/resource/avi/avi_archive_delegate.h"
#include "recording/stream_recorder.h"
#include "plugins/resource/avi/avi_resource.h"
#include "plugins/resource/archive/archive_stream_reader.h"
#include <QtCore/QDebug>
#include "recording_manager.h"
#include <media_server/serverutil.h>

#include "utils/common/synctime.h"

#include <recording/time_period.h>
#include <recording/time_period_list.h>

#include <boost/array.hpp>

DeviceFileCatalog::RebuildMethod DeviceFileCatalog::m_rebuildArchive = DeviceFileCatalog::Rebuild_None;
QMutex DeviceFileCatalog::m_rebuildMutex;
QSet<void*> DeviceFileCatalog::m_pauseList;

namespace {
    boost::array<QString, QnServer::ChunksCatalogCount> catalogPrefixes = {"low_quality", "hi_quality", "bookmarks"};
}

QString DeviceFileCatalog::prefixByCatalog(QnServer::ChunksCatalog catalog) {
    return catalogPrefixes[catalog];
}

QnServer::ChunksCatalog DeviceFileCatalog::catalogByPrefix(const QString &prefix) {
    for (int i = 0; i < QnServer::ChunksCatalogCount; ++i)
        if (catalogPrefixes[i] == prefix)
            return static_cast<QnServer::ChunksCatalog>(i);
    return QnServer::LowQualityCatalog; //default value
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
        return qBetween(startTimeMs, timeMs, endTimeMs());
}

void DeviceFileCatalog::Chunk::truncate(qint64 timeMs)
{
    durationMs = qMax(0ll, timeMs - startTimeMs);
}

DeviceFileCatalog::DeviceFileCatalog(const QString &cameraUniqueId, QnServer::ChunksCatalog catalog):
    m_mutex(QMutex::Recursive),
    m_cameraUniqueId(cameraUniqueId),
    m_catalog(catalog),
    m_lastAddIndex(-1),
    m_lastRecordRecording(false)
{
}

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
	QString prefix = rootFolder(storage, m_catalog); 

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
    for(const QFileInfo& info: itr.value().entryList)
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

qint64 DeviceFileCatalog::recreateFile(const QString& fileName, qint64 startTimeMs, const QnStorageResourcePtr &storage)
{
    NX_LOG(lit("recreate broken file %1").arg(fileName), cl_logWARNING);
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
    while( (packet = avi->getNextData()) )
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

void DeviceFileCatalog::replaceChunks(int storageIndex, const std::deque<Chunk>& newCatalog)
{
    QMutexLocker lock(&m_mutex);

    std::deque<Chunk> filteredData;
    filteredData.swap( m_chunks );
    const auto filteredDataEndIter = std::remove_if( filteredData.begin(), filteredData.end(),
        [storageIndex](const Chunk& chunk){ return chunk.storageIndex == storageIndex; } );
    m_chunks.resize( filteredDataEndIter - filteredData.begin() + newCatalog.size() );
    std::merge( filteredData.begin(), filteredDataEndIter, newCatalog.begin(), newCatalog.end(), m_chunks.begin() );
    m_lastAddIndex = -1;
}

QSet<QDate> DeviceFileCatalog::recordedMonthList()
{
    QMutexLocker lock(&m_mutex);

    QSet<QDate> rez;
    if (m_chunks.empty())
        return rez;

    std::deque<DeviceFileCatalog::Chunk>::iterator curItr = m_chunks.begin();
    QDate monthStart(1970, 1, 1);

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
    QMutexLocker lk( &m_mutex );

    if (!m_chunks.empty() && chunk.startTimeMs > m_chunks[m_chunks.size()-1].startTimeMs) {
        m_chunks.push_back(chunk);
    }
    else {
        ChunkMap::iterator itr = qUpperBound(m_chunks.begin(), m_chunks.end(), chunk.startTimeMs);
        m_chunks.insert(itr, chunk);
    }
    if (m_chunks.size() > 1 && m_chunks[m_chunks.size()-1].startTimeMs == m_chunks[m_chunks.size()-2].endTimeMs() + 1)
    {
        // update catalog to fix bug from previous version
        m_chunks[m_chunks.size()-2].durationMs = m_chunks[m_chunks.size()-1].startTimeMs - m_chunks[m_chunks.size()-2].startTimeMs;
        return true;
    }
    return false;
}

void DeviceFileCatalog::addChunks(const std::deque<Chunk>& chunks)
{
    QMutexLocker lk( &m_mutex );

    std::deque<Chunk> existChunks;
    existChunks.swap( m_chunks );
    m_chunks.resize(existChunks.size() + chunks.size());
    auto itr = std::set_union(existChunks.begin(), existChunks.end(), chunks.begin(), chunks.end(), m_chunks.begin());
    if (!m_chunks.empty())
        m_chunks.resize(itr - m_chunks.begin());


    m_lastAddIndex = -1;
}

std::deque<DeviceFileCatalog::Chunk> DeviceFileCatalog::mergeChunks(const std::deque<Chunk>& chunk1, const std::deque<Chunk>& chunk2)
{
    std::deque<Chunk> result;
    result.resize(chunk1.size() + chunk2.size());
    std::merge(chunk1.begin(), chunk1.end(), chunk2.begin(), chunk2.end(), result.begin());
    return result;
}

int DeviceFileCatalog::detectTimeZone(qint64 startTimeMs, const QString& fileName)
{
    int result = currentTimeZone()/60;

    QDateTime datetime1 = QDateTime::fromMSecsSinceEpoch(startTimeMs);
    datetime1 = datetime1.addMSecs(-(datetime1.time().minute()*60*1000ll + datetime1.time().second()*1000ll + datetime1.time().msec()));

    QStringList dateParts = fileName.split(QDir::separator());
    if (dateParts.size() < 5)
        return result;
    int hour = dateParts[dateParts.size()-2].toInt();
    int day = dateParts[dateParts.size()-3].toInt();
    int month = dateParts[dateParts.size()-4].toInt();
    int year = dateParts[dateParts.size()-5].toInt();

    QDateTime datetime2(QDate(year, month, day), QTime(hour, 0, 0));
    result += (datetime2.toMSecsSinceEpoch() - datetime1.toMSecsSinceEpoch()) / 1000 / 60;

    return result;
}

DeviceFileCatalog::Chunk DeviceFileCatalog::chunkFromFile(const QnStorageResourcePtr &storage, const QString& fileName)
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

        if (startTimeMs < 1 || endTimeMs - startTimeMs < 1) {
            delete avi;
            return chunk;
        }

        //chunk = Chunk(startTimeMs, storage->getIndex(), fileIndex, endTimeMs - startTimeMs, currentTimeZone()/60);
        chunk = Chunk(startTimeMs, storage->getIndex(), fileIndex, endTimeMs - startTimeMs, detectTimeZone(startTimeMs, fileName));
    }
    delete avi;
    return chunk;
}

QnTimePeriod DeviceFileCatalog::timePeriodFromDir(const QnStorageResourcePtr &storage, const QString& dirName)
{
    QnTimePeriod timePeriod;
    QString sUrl = storage->getPath();
    QStringList folders = dirName.mid(sUrl.size()).split('/').mid(3);

    QString timestamp(lit("%1/%2/%3T%4:00:00"));
    for (int i = 0; i < folders.size(); ++i)
        timestamp = timestamp.arg(folders[i].toInt(), i == 0 ? 4 : 2, 10, QChar('0'));
    for (int i = folders.size(); i < 4; ++i)
        timestamp = timestamp.arg(i == 3 ? 0 : 1, 2, 10, QChar('0')); // mm and dd from 1, hours from 0
    QDateTime dtStart = QDateTime::fromString(timestamp, Qt::ISODate);
    QDateTime dtEnd = dtStart;
    if (folders.size() == 4)
        dtEnd = dtEnd.addSecs(3600);
    else if (folders.size() == 3)
        dtEnd = dtEnd.addDays(1);
    else if (folders.size() == 2)
        dtEnd = dtEnd.addMonths(1);
    else if (folders.size() == 1)
        dtEnd = dtEnd.addYears(1);

    timePeriod.startTimeMs = dtStart.toMSecsSinceEpoch();
    timePeriod.durationMs = dtEnd.toMSecsSinceEpoch() - timePeriod.startTimeMs;

    return timePeriod;
}

void DeviceFileCatalog::rebuildPause(void* value)
{
    QMutexLocker lock(&m_rebuildMutex);
    m_pauseList << value;
}

void DeviceFileCatalog::rebuildResume(void* value)
{
    QMutexLocker lock(&m_rebuildMutex);
    m_pauseList.remove(value);
}

bool DeviceFileCatalog::needRebuildPause()
{
    QMutexLocker lock(&m_rebuildMutex);
    return !m_pauseList.isEmpty();
}

void DeviceFileCatalog::scanMediaFiles(const QString& folder, const QnStorageResourcePtr &storage, QMap<qint64, Chunk>& allChunks, QVector<EmptyFileInfo>& emptyFileList, const ScanFilter& filter)
{
    QDir dir(folder);
    for(const QFileInfo& fi: dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name))
    {
        while (m_rebuildArchive != Rebuild_None && needRebuildPause())
            QnLongRunnable::msleep(100);

        if (m_rebuildArchive == Rebuild_Canceled || QnResource::isStopping())
            return; // cancceled

        if (fi.isDir()) {
            QnTimePeriod folderPeriod = timePeriodFromDir(storage, fi.absoluteFilePath());
            if (!filter.isEmpty() && !filter.intersects(folderPeriod))
                continue;
            scanMediaFiles(fi.absoluteFilePath(), storage, allChunks, emptyFileList, filter);
        }
        else 
        {
            QString fileName = QDir::toNativeSeparators(fi.absoluteFilePath());

            Chunk chunk = chunkFromFile(storage, fileName);
            chunk.setFileSize(fi.size());
            if (!filter.isEmpty() && chunk.startTimeMs > filter.scanPeriod.endTimeMs())
                continue;

            if (chunk.durationMs > 0 && chunk.startTimeMs > 0) {
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
            else {
                //qnFileDeletor->deleteFile(fi.absoluteFilePath());
                emptyFileList << EmptyFileInfo(fi.created().toMSecsSinceEpoch(), fi.absoluteFilePath());
            }
        }

    }
}

void DeviceFileCatalog::readStorageData(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog, QMap<qint64, Chunk>& allChunks, QVector<EmptyFileInfo>& emptyFileList, const ScanFilter& scanFilter) 
{
    scanMediaFiles(rootFolder(storage, catalog), storage, allChunks, emptyFileList, scanFilter);
}
QString DeviceFileCatalog::rootFolder(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog) const 
{
    QString path = closeDirPath(storage->getPath());
    QString separator = getPathSeparator(path);

    return path + prefixByCatalog(catalog) + separator + m_cameraUniqueId + separator;
}

QString DeviceFileCatalog::cameraUniqueId() const {
    return m_cameraUniqueId;
}

bool DeviceFileCatalog::doRebuildArchive(const QnStorageResourcePtr &storage, const QnTimePeriod& period)
{
    QElapsedTimer t;
    t.restart();
    qWarning() << "start rebuilding archive for camera " << m_cameraUniqueId << prefixByCatalog(m_catalog);
    //m_rebuildStartTime = qnSyncTime->currentMSecsSinceEpoch();

    QMap<qint64, Chunk> allChunks;
    if (m_rebuildArchive == Rebuild_None || m_rebuildArchive == Rebuild_Canceled) {
        return false;
    }
    QVector<EmptyFileInfo> emptyFileList;
    readStorageData(storage, m_catalog, allChunks, emptyFileList, period);

    for(const EmptyFileInfo& emptyFile: emptyFileList) {
        if (emptyFile.startTimeMs < period.endTimeMs())
            qnFileDeletor->deleteFile(emptyFile.fileName);
    }

    QMutexLocker lk( &m_mutex );

    for(const Chunk& chunk: allChunks)
        m_chunks.push_back(chunk);

    qWarning() << "rebuild archive for camera " << m_cameraUniqueId << prefixByCatalog(m_catalog) << "finished. time=" << t.elapsed() << "ms. processd files=" << m_chunks.size();

    return true;
}

QnServer::ChunksCatalog DeviceFileCatalog::getRole() const
{
    return m_catalog; // it is a const data
}

void DeviceFileCatalog::addRecord(const Chunk& chunk)
{
    Q_ASSERT(chunk.durationMs < 1000 * 1000);

    QMutexLocker lock(&m_mutex);

    ChunkMap::iterator itr = qUpperBound(m_chunks.begin(), m_chunks.end(), chunk.startTimeMs);
    if( itr != m_chunks.end() )
    {
        itr = m_chunks.insert(itr, chunk);  //triggers assert if itr == end()
    }
    else
    {
        m_chunks.push_back( chunk );
        itr = m_chunks.begin() + (m_chunks.size()-1);
    }
    m_lastAddIndex = itr - m_chunks.begin();
}

void DeviceFileCatalog::removeRecord(int idx)
{
    if (idx == 0)
        m_chunks.pop_front();
    else
        m_chunks.erase(m_chunks.begin() + idx);
    if (m_lastAddIndex >= idx)
        m_lastAddIndex--;
}

DeviceFileCatalog::Chunk DeviceFileCatalog::takeChunk(qint64 startTimeMs, qint64 durationMs) {
    QMutexLocker lock(&m_mutex);
    
    ChunkMap::iterator itr = qUpperBound(m_chunks.begin(), m_chunks.end(), startTimeMs);
    if (itr > m_chunks.begin())
        --itr;
    while (itr > m_chunks.begin() && itr->startTimeMs == startTimeMs && itr->durationMs != durationMs)
        --itr;

    if (itr != m_chunks.end() && itr->startTimeMs == startTimeMs && itr->durationMs == durationMs) {
        Chunk result = *itr;
        int idx = itr - m_chunks.begin();
        removeRecord(idx);
        return result;
    }

    return Chunk();
}

qint64 DeviceFileCatalog::getLatRecordingTime() const
{
    QMutexLocker lock(&m_mutex);
    if (m_lastAddIndex >= 0)
        return m_chunks[m_lastAddIndex].startTimeMs;
    return -1;
}

void DeviceFileCatalog::setLatRecordingTime(qint64 value)
{
    QMutexLocker lock(&m_mutex);
    m_lastAddIndex = -1;
    m_lastRecordRecording = false;
    for (size_t i = 0; i < m_chunks.size(); ++i)
    {
        if (m_chunks[i].startTimeMs == value) {
            m_lastAddIndex = i;
            m_lastRecordRecording = true;
            break;
        }
    }
}

qint64 DeviceFileCatalog::lastChunkStartTime() const
{
    QMutexLocker lock(&m_mutex);
    return m_chunks.empty() ? 0 : m_chunks[m_chunks.size()-1].startTimeMs;
}

DeviceFileCatalog::Chunk DeviceFileCatalog::updateDuration(int durationMs, qint64 fileSize)
{
    Q_ASSERT(durationMs < 1000 * 1000);
    QMutexLocker lock(&m_mutex);
    //m_chunks.last().durationMs = durationMs;
    m_lastRecordRecording = false;
    if (m_lastAddIndex >= 0) {
        m_chunks[m_lastAddIndex].durationMs = durationMs;
        m_chunks[m_lastAddIndex].setFileSize(fileSize);
        return m_chunks[m_lastAddIndex];
    }
    else {
        return Chunk();
    }
}

/*
QVector<qint64> DeviceFileCatalog::deleteRecordBeforeDays(int days)
{
    static const qint64 MSECS_PER_DAY = 1000ll * 3600ll * 24ll;
    qint64 deleteThreshold = qnSyncTime->currentMSecsSinceEpoch() - MSECS_PER_DAY * days;
    auto deleteItr = qUpperBound(m_chunks.begin(), m_chunks.end(), deleteThreshold);
    if (deleteItr != m_chunks.end()) 
        return deleteRecordsBefore(deleteItr - m_chunks.begin());
    else
        return QVector<qint64>();
}
*/

QVector<DeviceFileCatalog::Chunk> DeviceFileCatalog::deleteRecordsBefore(int idx)
{
    int count = idx;
    QVector<Chunk> result;
    for (int i = 0; i < count; ++i) {
        Chunk deletedChunk = deleteFirstRecord();
        if (deletedChunk.startTimeMs)
            result << deletedChunk;
    }
    return result;
}

void DeviceFileCatalog::clear()
{
    QMutexLocker lk( &m_mutex );
    while(!m_chunks.empty())
    {
        lk.unlock();
        deleteFirstRecord();
        lk.relock();
    }
}

void DeviceFileCatalog::deleteRecordsByStorage(int storageIndex, qint64 timeMs)
{
    QMutexLocker lock(&m_mutex);

    for (size_t i = 0; i < m_chunks.size();)
    {
        if (m_chunks[i].storageIndex == storageIndex)
        {
            if (m_chunks[i].startTimeMs < timeMs) {
                removeRecord(i);
            }
            else
                break;

        }
        else {
            ++i;
        }
    }
}

bool DeviceFileCatalog::isEmpty() const
{
    QMutexLocker lock(&m_mutex);
    return m_chunks.empty();
}

DeviceFileCatalog::Chunk DeviceFileCatalog::deleteFirstRecord()
{
    QnStorageResourcePtr storage;
    QString delFileName;
    Chunk deletedChunk;
    {
        QMutexLocker lock(&m_mutex);

        if (m_chunks.empty())
            return deletedChunk;

        if (!m_chunks.empty()) 
        {
            storage = qnStorageMan->storageRoot(m_chunks[0].storageIndex);
            delFileName = fullFileName(m_chunks[0]);
            deletedChunk = m_chunks[0];

            removeRecord(0);
        }
    }

    if (storage) {
        if (!delFileName.isEmpty())
        {
            //storage->addWritedSpace(-deletedSize);
            storage->removeFile(delFileName);
        }
    }

    return deletedChunk;
}

int DeviceFileCatalog::findFileIndex(qint64 startTimeMs, FindMethod method) const
{
/*
    QString msg;
    QTextStream str(&msg);
    str << " find chunk for time=" << QDateTime::fromMSecsSinceEpoch(startTime/1000).toString();
    str.flush();
    NX_LOG(msg, cl_logWARNING);
    str.flush();
*/
    QMutexLocker lock(&m_mutex);

    if (m_chunks.empty())
        return -1;

    ChunkMap::const_iterator itr = qUpperBound(m_chunks.begin(), m_chunks.end(), startTimeMs);
    if (itr > m_chunks.begin())
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
    ChunkMap::const_iterator itr = qLowerBound(m_chunks.begin(), m_chunks.end(), chunk.startTimeMs);
    if (itr != m_chunks.end() && itr->startTimeMs == chunk.startTimeMs)
        chunk.durationMs = itr->durationMs;
}

QString DeviceFileCatalog::fullFileName(const Chunk& chunk) const
{
    QnStorageResourcePtr storage = qnStorageMan->storageRoot(chunk.storageIndex);
    if (!storage)
        return QString();

    QString root = rootFolder(storage, m_catalog);
    QString separator = getPathSeparator(root);
    return root +
                QnStorageManager::dateTimeStr(chunk.startTimeMs, chunk.timeZone, separator) + 
                strPadLeft(QString::number(chunk.fileIndex), 3, '0') + 
                QString(".mkv");
}

qint64 DeviceFileCatalog::minTime() const
{
    QMutexLocker lock(&m_mutex);
    if (m_chunks.empty())
        return AV_NOPTS_VALUE;
    else
        return m_chunks[0].startTimeMs;
}

qint64 DeviceFileCatalog::maxTime() const
{
    QMutexLocker lock(&m_mutex);
    if (m_chunks.empty())
        return AV_NOPTS_VALUE;
    else if (m_lastAddIndex >= 0 && m_chunks[m_lastAddIndex].durationMs == -1)
        return DATETIME_NOW;
    else if (m_chunks[m_chunks.size()-1].durationMs == -1)
        return DATETIME_NOW;
    else
        return m_chunks[m_chunks.size()-1].startTimeMs + qMax(0, m_chunks[m_chunks.size()-1].durationMs);
}

bool DeviceFileCatalog::containTime(qint64 timeMs, qint64 eps) const
{
    QMutexLocker lk( &m_mutex );

    if (m_chunks.empty())
        return false;

    ChunkMap::const_iterator itr = qUpperBound(m_chunks.begin(), m_chunks.end(), timeMs);
    if (itr != m_chunks.end() && itr->startTimeMs - timeMs <= eps)
        return true;
    if (itr > m_chunks.begin())
        --itr;
    return itr->distanceToTime(timeMs) <= eps;
}

bool DeviceFileCatalog::isLastChunk(qint64 startTimeMs) const
{
    QMutexLocker lock(&m_mutex);
    if (m_chunks.empty())    
        return true;
    else
        return m_chunks[m_chunks.size()-1].startTimeMs == startTimeMs;
}

DeviceFileCatalog::Chunk DeviceFileCatalog::chunkAt(int index) const
{
    QMutexLocker lock(&m_mutex);
    if (index >= 0 && (size_t)index < m_chunks.size() )
        return m_chunks.at(index);
    else
        return DeviceFileCatalog::Chunk();
}

qint64 DeviceFileCatalog::firstTime() const
{
    QMutexLocker lock(&m_mutex);
    if (m_chunks.empty())
        return AV_NOPTS_VALUE;
    else
        return m_chunks[0].startTimeMs;
}

void DeviceFileCatalog::setRebuildArchive(RebuildMethod value)
{
    m_rebuildArchive = value;
}

void DeviceFileCatalog::close()
{
}

QnTimePeriodList DeviceFileCatalog::getTimePeriods(qint64 startTime, qint64 endTime, qint64 detailLevel)
{
    //qDebug() << "find period from " << QDateTime::fromMSecsSinceEpoch(startTime).toString("hh:mm:ss.zzz") << "to" << QDateTime::fromMSecsSinceEpoch(endTime).toString("hh:mm:ss.zzz");

    QMutexLocker lock(&m_mutex);
    QnTimePeriodList result;
    if (m_chunks.empty())
        return result;

    ChunkMap::const_iterator itr = qLowerBound(m_chunks.begin(), m_chunks.end(), startTime);
    /* Checking if we should include a chunk, containing startTime. */
    if (itr != m_chunks.begin()) {
        --itr;

        /* Case if previous chunk does not contain startTime. */
        if (itr->endTimeMs() < startTime)
            ++itr;
    }

    if (itr == m_chunks.end())
        return result;

    size_t firstIndex = itr - m_chunks.begin();
    result << QnTimePeriod(m_chunks[firstIndex].startTimeMs, m_chunks[firstIndex].durationMs);

    for (size_t i = firstIndex+1; i < m_chunks.size() && m_chunks[i].startTimeMs < endTime; ++i)
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

bool DeviceFileCatalog::fromCSVFile(const QString& fileName)
{
    QFile file(fileName);

    if (!file.open(QFile::ReadOnly))
    {
        NX_LOG(lit("Can't open title file %1").arg(file.fileName()), cl_logERROR);
        return false;
    }

    // deserializeTitleFile()

    bool needRewriteCatalog = false;

    int timeZoneExist = 0;
    QByteArray headerLine = file.readLine();
    if (headerLine.contains("timezone"))
        timeZoneExist = 1;
    QByteArray line;
    bool checkDirOnly = false;
    do {
        line = file.readLine();
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
            needRewriteCatalog |= addChunk(chunk);
        }
        else {
            needRewriteCatalog = true;
        }

    } while (!line.isEmpty());
    
    if (!timeZoneExist)
        needRewriteCatalog = true; // update catalog to new version


    return true;
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

bool DeviceFileCatalog::ScanFilter::intersects(const QnTimePeriod& period) const {
    return QnTimePeriod(scanPeriod.startTimeMs, scanPeriod.durationMs).intersects(period);
}
