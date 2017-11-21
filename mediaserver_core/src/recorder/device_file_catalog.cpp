#include "device_file_catalog.h"

#include <array>

#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>

#include <core/resource/resource.h>
#include <core/resource/network_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource_management/resource_pool.h>

#include "storage_manager.h"
#include "utils/common/util.h"
#include <utils/fs/file.h>
#include <nx/utils/log/log.h>
#include <utils/math/math.h>
#include "recorder/file_deletor.h"
#include "plugins/resource/avi/avi_archive_delegate.h"
#include "recording/stream_recorder.h"
#include "plugins/resource/avi/avi_resource.h"
#include "nx/streaming/archive_stream_reader.h"
#include <QtCore/QDebug>
#include "recording_manager.h"
#include <media_server/serverutil.h>
#include <database/server_db.h>

#include "utils/common/synctime.h"

#include <recording/time_period.h>
#include <recording/time_period_list.h>

#include <nx/utils/concurrent.h>
#include "storage_db_pool.h"

QnMutex DeviceFileCatalog::m_rebuildMutex;
QSet<void*> DeviceFileCatalog::m_pauseList;

const quint16 DeviceFileCatalog::Chunk::FILE_INDEX_NONE = 0xffff;
const quint16 DeviceFileCatalog::Chunk::FILE_INDEX_WITH_DURATION = 0xfffe;
const int DeviceFileCatalog::Chunk::UnknownDuration = -1;

namespace {
    std::array<QString, QnServer::ChunksCatalogCount> catalogPrefixes = {"low_quality", "hi_quality"};

    QString toLocalStoragePath(const QnStorageResourcePtr &storage, const QString& absolutePath)
    {
        QString sUrl = storage->getUrl();
        return absolutePath.mid(sUrl.size());
    }
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

void DeviceFileCatalog::TruncableChunk::truncate(qint64 timeMs)
{
    if (!isTruncated)
    {
        originalDuration = durationMs;
        isTruncated = true;
    }
    durationMs = qMax(0ll, timeMs - startTimeMs);
}

DeviceFileCatalog::DeviceFileCatalog(
    const QString           &cameraUniqueId,
    QnServer::ChunksCatalog catalog,
    QnServer::StoragePool   storagePool
) :
    m_mutex(QnMutex::Recursive),
    m_cameraUniqueId(cameraUniqueId),
    m_catalog(catalog),
    m_recordingChunkTime(-1),
    m_storagePool(storagePool),
    m_lastSyncTime(0)
{
}

qint64 DeviceFileCatalog::getSpaceByStorageIndex(int storageIndex) const
{
    qint64 result = 0;
    QnMutexLocker lock(&m_mutex);

    for (const auto& chunk: m_chunks)
    {
        if (chunk.storageIndex == storageIndex)
            result += chunk.getFileSize();
    }

    return result;
}

QString getDirName(const QString& prefix, int currentParts[4], int i)
{
    QString result = prefix;
    for (int j = 0; j <= i; ++j)
        result += strPadLeft(QString::number(currentParts[j]), 2, '0') + QString('/');
    return result;
}

bool DeviceFileCatalog::csvMigrationCheckFile(const Chunk& chunk, QnStorageResourcePtr storage)
{
	QString prefix = rootFolder(storage, m_catalog);

    QDateTime fileDate = QDateTime::fromMSecsSinceEpoch(chunk.startTimeMs);
    if (chunk.timeZone != -1)
        fileDate = fileDate.toUTC().addSecs(chunk.timeZone*60);

    int currentParts[4];
    currentParts[0] = fileDate.date().year();
    currentParts[1] = fileDate.date().month();
    currentParts[2] = fileDate.date().day();
    currentParts[3] = fileDate.time().hour();

    //bool sameDir = true;

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
            //sameDir = false;
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

    return true;

    /*
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
    */
}

void DeviceFileCatalog::removeChunks(int storageIndex)
{
    replaceChunks(storageIndex, std::deque<Chunk>());
}

void DeviceFileCatalog::replaceChunks(int storageIndex, const std::deque<Chunk>& newCatalog)
{
    NX_ASSERT(std::is_sorted(newCatalog.begin(), newCatalog.end()));

    QnMutexLocker lock( &m_mutex );

    std::deque<Chunk> filteredData;
    filteredData.swap( m_chunks );
    const auto filteredDataEndIter = std::remove_if( filteredData.begin(), filteredData.end(),
        [storageIndex](const Chunk& chunk){ return chunk.storageIndex == storageIndex; } );
    m_chunks.resize( filteredDataEndIter - filteredData.begin() + newCatalog.size() );
    std::merge( filteredData.begin(), filteredDataEndIter, newCatalog.begin(), newCatalog.end(), m_chunks.begin() );
}

QSet<QDate> DeviceFileCatalog::recordedMonthList()
{
    QnMutexLocker lock( &m_mutex );

    QSet<QDate> rez;
    if (m_chunks.empty())
        return rez;

    std::deque<DeviceFileCatalog::Chunk>::iterator curItr = m_chunks.begin();
    QDate monthStart(1970, 1, 1);

    curItr = std::lower_bound(curItr, m_chunks.end(), QDateTime(monthStart).toMSecsSinceEpoch());
    while (curItr != m_chunks.end())
    {
        monthStart = QDateTime::fromMSecsSinceEpoch(curItr->startTimeMs).date();
        monthStart = monthStart.addDays(1 - monthStart.day());
        rez << monthStart;
        monthStart = monthStart.addMonths(1);
        curItr = std::lower_bound(curItr, m_chunks.end(), QDateTime(monthStart).toMSecsSinceEpoch());
    }
    return rez;
}

bool DeviceFileCatalog::addChunk(const Chunk& chunk)
{
    QnMutexLocker lk( &m_mutex );

    if (!m_chunks.empty() && chunk.startTimeMs > m_chunks[m_chunks.size()-1].startTimeMs) {
        m_chunks.push_back(chunk);
    }
    else {
        ChunkMap::iterator itr = std::upper_bound(m_chunks.begin(), m_chunks.end(), chunk.startTimeMs);
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
    NX_ASSERT(std::is_sorted(chunks.begin(), chunks.end()));

    QnMutexLocker lk( &m_mutex );

    std::deque<Chunk> existChunks;
    existChunks.swap( m_chunks );
    m_chunks.resize(existChunks.size() + chunks.size());
    auto itr = std::set_union(existChunks.begin(), existChunks.end(), chunks.begin(), chunks.end(), m_chunks.begin());
    if (!m_chunks.empty())
        m_chunks.resize(itr - m_chunks.begin());
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
    QDateTime datetime1 = QDateTime::fromMSecsSinceEpoch(startTimeMs).toUTC();
    datetime1 = datetime1.addMSecs(-(datetime1.time().minute()*60*1000ll + datetime1.time().second()*1000ll + datetime1.time().msec()));

    QStringList dateParts = fileName.split(getPathSeparator(fileName));
    if (dateParts.size() < 5)
        return currentTimeZone()/60;
    int hour = dateParts[dateParts.size()-2].toInt();
    int day = dateParts[dateParts.size()-3].toInt();
    int month = dateParts[dateParts.size()-4].toInt();
    int year = dateParts[dateParts.size()-5].toInt();

    QDateTime datetime2 = QDateTime(QDate(year, month, day), QTime(hour, 0, 0), Qt::UTC);
    int result = (datetime2.toMSecsSinceEpoch() - datetime1.toMSecsSinceEpoch()) / 1000 / 60;

    return result;
}

DeviceFileCatalog::Chunk DeviceFileCatalog::chunkFromFile(
    const QnStorageResourcePtr  &storage,
    const QString               &fileName
)
{
    Chunk chunk;
    if (fileName.indexOf(lit(".txt")) != -1)
        return chunk;

    const QString localFileName = toLocalStoragePath(storage, fileName);

    if (QnFile::baseName(fileName).indexOf(lit("_")) == -1)
    {
        QnAviResourcePtr res(new QnAviResource(fileName));
        QnAviArchiveDelegate* avi = new QnAviArchiveDelegate();
        avi->setStorage(storage);
        avi->setFastStreamFind(true);
        if (avi->open(res) && avi->findStreams() && avi->endTime() != (qint64)AV_NOPTS_VALUE)
        {
            qint64 startTimeMs = avi->startTime()/1000;
            qint64 endTimeMs = avi->endTime()/1000;
            QString baseName = QnFile::baseName(fileName);
            int fileIndex = baseName.length() <= 3 ? baseName.toInt() : Chunk::FILE_INDEX_NONE;

            if (startTimeMs < 1 || endTimeMs - startTimeMs < 1) {
                delete avi;
                return chunk;
            }
            chunk = Chunk(
                startTimeMs,
                qnStorageDbPool->getStorageIndex(storage),
                fileIndex,
                endTimeMs - startTimeMs,
                detectTimeZone(startTimeMs, localFileName)
            );
        }
        else {
            qWarning() << "Can't open media file" << fileName << "storage=" << storage->getUrl();
        }
        delete avi;
        return chunk;
    }

    if (!storage->isFileExists(fileName))
        return chunk;

    auto    nameParts   = QnFile::baseName(fileName).split(lit("_"));
    int64_t startTimeMs = nameParts[0].toLongLong();
    int64_t durationMs  = nameParts[1].toInt();
    int     fileIndex   = nameParts[0].length() <= 3 ? nameParts[0].toInt() :
                                                       nameParts.size() == 2 ? Chunk::FILE_INDEX_WITH_DURATION :
                                                                               Chunk::FILE_INDEX_NONE;
    if (startTimeMs < 1 || durationMs < 1)
        return chunk;

    chunk = Chunk(
        startTimeMs,
        qnStorageDbPool->getStorageIndex(storage),
        fileIndex,
        durationMs,
        detectTimeZone(startTimeMs, localFileName)
    );
    return chunk;
}

QnStorageManager *DeviceFileCatalog::getMyStorageMan() const
{
    if (m_storagePool == QnServer::StoragePool::Normal)
        return qnNormalStorageMan;
    else if (m_storagePool == QnServer::StoragePool::Backup)
        return qnBackupStorageMan;
    else {
        NX_ASSERT(0);
        return nullptr;
    }
}

QnServer::StoragePool DeviceFileCatalog::getStoragePool() const
{
    QnMutexLocker lk(&m_mutex);
    return m_storagePool;
}

QnTimePeriod DeviceFileCatalog::timePeriodFromDir(
    const QnStorageResourcePtr  &storage,
    const QString               &dirName
)
{
    QnTimePeriod timePeriod;
    const QString path = toLocalStoragePath(storage, dirName);
    QStringList folders = path.split(getPathSeparator(path)).mid(3);

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
    QnMutexLocker lock( &m_rebuildMutex );
    if (m_pauseList.isEmpty())
        qWarning() << "Temporary disable rebuild archive (if active) due to high disk usage slow down recorders";
    m_pauseList << value;
}

void DeviceFileCatalog::rebuildResume(void* value)
{
    QnMutexLocker lock( &m_rebuildMutex );
    if (!m_pauseList.remove(value))
        return;
    if (m_pauseList.isEmpty())
        qWarning() << "Rebuild archive is allowed again (if active) due to disk performance is OK";
}

bool DeviceFileCatalog::needRebuildPause()
{
    QnMutexLocker lock( &m_rebuildMutex );
    return !m_pauseList.isEmpty();
}

void DeviceFileCatalog::scanMediaFiles(const QString& folder, const QnStorageResourcePtr &storage, QMap<qint64, Chunk>& allChunks, QVector<EmptyFileInfo>& emptyFileList, const ScanFilter& filter)
{
//    qDebug() << "folder being scanned: " << folder;
    NX_LOG(lit("%1 Processing directory %2").arg(Q_FUNC_INFO).arg(folder), cl_logDEBUG2);
    QnAbstractStorageResource::FileInfoList files;

    for(const QnAbstractStorageResource::FileInfo& fi: storage->getFileList(folder))
    {
        while (!getMyStorageMan()->needToStopMediaScan() && needRebuildPause())
            QnLongRunnable::msleep(100);

        if (getMyStorageMan()->needToStopMediaScan() || QnResource::isStopping())
        {
            NX_LOG(lit("%1 Stop requested. Cancelling").arg(Q_FUNC_INFO), cl_logDEBUG2);
            return; // canceled
        }

        if (fi.isDir()) {
            QnTimePeriod folderPeriod = timePeriodFromDir(storage, fi.absoluteFilePath());
            if (!filter.isEmpty() && !filter.intersects(folderPeriod))
                continue;
            scanMediaFiles(fi.absoluteFilePath(), storage, allChunks, emptyFileList, filter);
        }
        else
        {
            files << fi;
        }
    }

    if (files.empty())
        return;

    NX_INFO(this, lm("[Scan] started for directory: %1").args(folder));

    QThreadPool tp;
    tp.setMaxThreadCount(4);
    QnMutex scanFilesMutex;
    for(const auto& fi: files)
    {
        if (QnResource::isStopping())
        {
            NX_LOG(lit("%1 Stop requested. Cancelling").arg(Q_FUNC_INFO), cl_logDEBUG2);
            break;
        }

        nx::utils::concurrent::run( &tp, [&]()
        {
            //QString fileName = QDir::toNativeSeparators(fi.absoluteFilePath());
            QString fileName = fi.absoluteFilePath();
            qint64 fileTime = QFileInfo(fileName).baseName().toLongLong();

            if (!filter.isEmpty() && fileTime > filter.scanPeriod.endTimeMs())
                return;

            Chunk chunk = chunkFromFile(storage, fileName);
            chunk.setFileSize(fi.size());
            if (!filter.isEmpty() && chunk.startTimeMs > filter.scanPeriod.endTimeMs())
            {
                return;
            }

            QnMutexLocker lock(&scanFilesMutex);
            if (chunk.durationMs > 0 && chunk.startTimeMs > 0) {
                QMap<qint64, Chunk>::iterator itr = allChunks.insert(chunk.startTimeMs, chunk);
                if (itr != allChunks.begin())
                {
                    Chunk& prevChunk = *(itr-1);
                    qint64 delta = chunk.startTimeMs - prevChunk.endTimeMs();
                    if (delta < MAX_FRAME_DURATION && !fi.baseName().contains("_") /*Old version file*/)
                        prevChunk.durationMs = chunk.startTimeMs - prevChunk.startTimeMs;
                }

                if (allChunks.size() % 1000 == 0)
                    qWarning() << allChunks.size() << "media files processed...";

            }
            else if (fi.fileName().indexOf(".txt") == -1) {
                qDebug() << "remove file" << fi.absoluteFilePath() << "because of empty chunk. duration=" << chunk.durationMs << "startTime=" << chunk.startTimeMs;
                emptyFileList
                    << EmptyFileInfo(/*fi.created().toMSecsSinceEpoch()*/
                           chunk.startTimeMs,
                           fi.absoluteFilePath()
                       );
            }
        }
        );
    }
    tp.waitForDone();

    NX_INFO(this, lm("[Scan] finished for directory: %1, %2 files processed").args(folder, allChunks.size()));
}

void DeviceFileCatalog::readStorageData(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog, QMap<qint64, Chunk>& allChunks, QVector<EmptyFileInfo>& emptyFileList, const ScanFilter& scanFilter)
{
    scanMediaFiles(
        rootFolder(storage, catalog),
        storage,
        allChunks,
        emptyFileList,
        scanFilter
    );
}

QString DeviceFileCatalog::rootFolder(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog) const
{
    QString path = closeDirPath(storage->getUrl());
    QString separator = getPathSeparator(path);

    return path + prefixByCatalog(catalog) + separator + m_cameraUniqueId + separator;
}

QString DeviceFileCatalog::cameraUniqueId() const {
    return m_cameraUniqueId;
}

bool DeviceFileCatalog::doRebuildArchive(const QnStorageResourcePtr &storage, const QnTimePeriod& period)
{
//     QElapsedTimer t;
//     t.restart();
//     qWarning() << "start rebuilding archive for camera " << m_cameraUniqueId << prefixByCatalog(m_catalog);
    //m_rebuildStartTime = qnSyncTime->currentMSecsSinceEpoch();

    QMap<qint64, Chunk> allChunks;
    if (getMyStorageMan()->needToStopMediaScan())
        return false;

    QVector<EmptyFileInfo> emptyFileList;
    readStorageData(
        storage,
        m_catalog,
        allChunks,
        emptyFileList,
        period
    );

    for(const EmptyFileInfo& emptyFile: emptyFileList) {
        if (emptyFile.startTimeMs < period.endTimeMs())
            storage->removeFile(emptyFile.fileName);
    }

    QnMutexLocker lk( &m_mutex );

    for(const Chunk& chunk: allChunks)
        m_chunks.push_back(chunk);

    //qWarning() << "rebuild archive for camera " << m_cameraUniqueId << prefixByCatalog(m_catalog) << "finished. time=" << t.elapsed() << "ms. processd files=" << m_chunks.size();

    return true;
}

QnServer::ChunksCatalog DeviceFileCatalog::getRole() const
{
    return m_catalog; // it is a const data
}

void DeviceFileCatalog::addRecord(const Chunk& chunk)
{
    NX_ASSERT(chunk.durationMs < 1000 * 1000);

    QnMutexLocker lock( &m_mutex );

    ChunkMap::iterator itr = std::upper_bound(m_chunks.begin(), m_chunks.end(), chunk.startTimeMs);
    if( itr != m_chunks.end() )
    {
        itr = m_chunks.insert(itr, chunk);  //triggers NX_ASSERT if itr == end()
    }
    else
    {
        m_chunks.push_back( chunk );
        itr = m_chunks.begin() + (m_chunks.size()-1);
    }
    m_recordingChunkTime = chunk.startTimeMs;
}

void DeviceFileCatalog::removeRecord(int idx)
{
    if (idx == 0)
        m_chunks.pop_front();
    else
        m_chunks.erase(m_chunks.begin() + idx);
}

DeviceFileCatalog::Chunk DeviceFileCatalog::takeChunk(qint64 startTimeMs, qint64 durationMs) {
    QnMutexLocker lock( &m_mutex );

    ChunkMap::iterator itr = std::upper_bound(m_chunks.begin(), m_chunks.end(), startTimeMs);
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

qint64 DeviceFileCatalog::lastChunkStartTime() const
{
    QnMutexLocker lock( &m_mutex );
    return m_chunks.empty() ? 0 : m_chunks[m_chunks.size()-1].startTimeMs;
}

qint64 DeviceFileCatalog::lastChunkStartTime(int storageIndex) const
{
    QnMutexLocker lock( &m_mutex );
    for (auto itr = m_chunks.rbegin(); itr != m_chunks.rend(); ++itr)
    {
        if (itr->storageIndex == storageIndex)
            return itr->startTimeMs;
    }
    return 0;
}

DeviceFileCatalog::Chunk DeviceFileCatalog::updateDuration(int durationMs, qint64 fileSize, bool indexWithDuration)
{
    NX_ASSERT(durationMs < 1000 * 1000);
    QnMutexLocker lock( &m_mutex );
    //m_chunks.last().durationMs = durationMs;
    auto itr = std::lower_bound(m_chunks.begin(), m_chunks.end(), m_recordingChunkTime);
    if (itr != m_chunks.end() && itr->startTimeMs == m_recordingChunkTime)
    {
        DeviceFileCatalog::Chunk& chunk = *itr;
        chunk.durationMs = durationMs;
        chunk.setFileSize(fileSize);
        if (indexWithDuration)
            chunk.fileIndex = Chunk::FILE_INDEX_WITH_DURATION;
        return chunk;
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
    auto deleteItr = std::upper_bound(m_chunks.begin(), m_chunks.end(), deleteThreshold);
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
    for (int i = 0; i < count && !m_chunks.empty(); ++i)
    {
        Chunk deletedChunk = deleteFirstRecord();
        if (deletedChunk.startTimeMs)
            result << deletedChunk;
    }
    return result;
}

void DeviceFileCatalog::clear()
{
    QnMutexLocker lk( &m_mutex );
    while(!m_chunks.empty())
    {
        lk.unlock();
        deleteFirstRecord();
        lk.relock();
    }
}

void DeviceFileCatalog::deleteRecordsByStorage(int storageIndex, qint64 timeMs)
{
    QnMutexLocker lock( &m_mutex );

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
    QnMutexLocker lock( &m_mutex );
    return m_chunks.empty();
}

DeviceFileCatalog::Chunk DeviceFileCatalog::deleteFirstRecord()
{
    QnStorageResourcePtr storage;
    QString delFileName;
    int storageIndex;
    Chunk deletedChunk;
    {
        QnMutexLocker lock( &m_mutex );

        if (m_chunks.empty())
            return deletedChunk;
        else
        {
            storageIndex = m_chunks[0].storageIndex;
            delFileName = fullFileName(m_chunks[0]);
            deletedChunk = m_chunks[0];

            removeRecord(0);
        }
    }

    storage = getMyStorageMan()->storageRoot(storageIndex);
    if (storage) {
        if (!delFileName.isEmpty())
        {
            //storage->addWritedSpace(-deletedSize);
            storage->removeFile(delFileName);
        }
    }

    return deletedChunk;
}

int DeviceFileCatalog::findNextFileIndex(qint64 startTimeMs) const
{
    QnMutexLocker lock( &m_mutex );
    if (m_chunks.empty())
        return -1;
    ChunkMap::const_iterator itr = std::upper_bound(
        m_chunks.begin(),
        m_chunks.end(),
        startTimeMs
    );
    return itr == m_chunks.end() ? -1 : itr - m_chunks.begin();
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
    QnMutexLocker lock( &m_mutex );

    if (m_chunks.empty())
        return -1;

    ChunkMap::const_iterator itr = std::upper_bound(m_chunks.begin(), m_chunks.end(), startTimeMs);
    if (itr > m_chunks.begin())
    {
        --itr;
         if (method == OnRecordHole_NextChunk && itr->startTimeMs + itr->durationMs <= startTimeMs)
         {
             if (itr < m_chunks.end()-1)
                 ++itr;
             else
                 return -1;
         }
    }
    else if (method == OnRecordHole_PrevChunk && startTimeMs <= itr->startTimeMs)
    {
        return -1;
    }

    return itr - m_chunks.begin();
}

void DeviceFileCatalog::updateChunkDuration(Chunk& chunk)
{
    QnMutexLocker lock( &m_mutex );
    ChunkMap::const_iterator itr = std::lower_bound(m_chunks.begin(), m_chunks.end(), chunk.startTimeMs);
    if (itr != m_chunks.end() && itr->startTimeMs == chunk.startTimeMs)
        chunk.durationMs = itr->durationMs;
}

QString DeviceFileCatalog::Chunk::fileName() const
{
    QString baseName = (fileIndex != FILE_INDEX_NONE && fileIndex != FILE_INDEX_WITH_DURATION) ?
                       strPadLeft(QString::number(fileIndex), 3, '0') :
                       QString::number(startTimeMs) +
                       (fileIndex == FILE_INDEX_WITH_DURATION ? lit("_") + QString::number(durationMs) :
                                                                lit(""));

    return baseName + QString(".mkv");
}

QString DeviceFileCatalog::fileDir(const Chunk& chunk) const
{
    QnStorageResourcePtr storage = getMyStorageMan()->storageRoot(chunk.storageIndex);
    if (!storage)
        return QString();

    QString root = rootFolder(storage, m_catalog);
    QString separator = getPathSeparator(root);

    return root + QnStorageManager::dateTimeStr(chunk.startTimeMs, chunk.timeZone, separator);
}

QString DeviceFileCatalog::fullFileName(const Chunk& chunk) const
{
    return fileDir(chunk) + chunk.fileName();
}

qint64 DeviceFileCatalog::minTime() const
{
    QnMutexLocker lock( &m_mutex );
    if (m_chunks.empty())
        return AV_NOPTS_VALUE;
    else
        return m_chunks[0].startTimeMs;
}

qint64 DeviceFileCatalog::maxTime() const
{
    QnMutexLocker lock( &m_mutex );
    if (m_chunks.empty())
        return AV_NOPTS_VALUE;
    else if (m_chunks[m_chunks.size()-1].durationMs == -1)
        return DATETIME_NOW;
    else
        return m_chunks[m_chunks.size()-1].startTimeMs + qMax(0, m_chunks[m_chunks.size()-1].durationMs);
}

bool DeviceFileCatalog::containTime(qint64 timeMs, qint64 eps) const
{
    QnMutexLocker lk( &m_mutex );

    if (m_chunks.empty())
        return false;

    ChunkMap::const_iterator itr = std::upper_bound(m_chunks.begin(), m_chunks.end(), timeMs);
    if (itr != m_chunks.end() && itr->startTimeMs - timeMs <= eps)
        return true;
    if (itr > m_chunks.begin())
        --itr;
    return itr->distanceToTime(timeMs) <= eps;
}

bool DeviceFileCatalog::containTime(const QnTimePeriod& period) const
{
    QnMutexLocker lk( &m_mutex );

    if (m_chunks.empty())
        return false;

    ChunkMap::const_iterator itr = std::lower_bound(m_chunks.begin(), m_chunks.end(), period.startTimeMs);
    if (itr != m_chunks.end() && itr->startTimeMs < period.endTimeMs())
        return true;
    if (itr == m_chunks.begin())
        return false;
    --itr;
    return itr->endTimeMs() >= period.startTimeMs;
}

bool DeviceFileCatalog::isLastChunk(qint64 startTimeMs) const
{
    QnMutexLocker lock( &m_mutex );
    if (m_chunks.empty())
        return true;
    else
        return m_chunks[m_chunks.size()-1].startTimeMs == startTimeMs;
}

DeviceFileCatalog::Chunk DeviceFileCatalog::chunkAt(int index) const
{
    QnMutexLocker lock( &m_mutex );
    if (index >= 0 && (size_t)index < m_chunks.size() )
        return m_chunks.at(index);
    else
        return DeviceFileCatalog::Chunk();
}

qint64 DeviceFileCatalog::firstTime() const
{
    QnMutexLocker lock( &m_mutex );
    if (m_chunks.empty())
        return AV_NOPTS_VALUE;
    else
        return m_chunks[0].startTimeMs;
}

void DeviceFileCatalog::close()
{
}

QnTimePeriodList DeviceFileCatalog::getTimePeriods(qint64 startTime, qint64 endTime, qint64 detailLevel, bool keepSmalChunks, int limit)
{
    //qDebug() << "find period from " << QDateTime::fromMSecsSinceEpoch(startTime).toString("hh:mm:ss.zzz") << "to" << QDateTime::fromMSecsSinceEpoch(endTime).toString("hh:mm:ss.zzz");

    QnMutexLocker lock( &m_mutex );
    QnTimePeriodList result;
    if (m_chunks.empty())
        return result;

    ChunkMap::const_iterator itr = std::lower_bound(m_chunks.begin(), m_chunks.end(), startTime);
    /* Checking if we should include a chunk, containing startTime. */
    if (itr != m_chunks.begin()) {
        --itr;

        /* Case if previous chunk does not contain startTime. */
        if (itr->endTimeMs() <= startTime)
            ++itr;
    }

    if (itr == m_chunks.end() || itr->startTimeMs >= endTime)
        return result;

    result.push_back(QnTimePeriod(itr->startTimeMs, itr->durationMs));

    ++itr;
    for (; itr != m_chunks.end() && itr->startTimeMs < endTime; ++itr)
    {
        QnTimePeriod& last = result.last();
        qint64 lastEndTime = last.startTimeMs + last.durationMs;
        if (lastEndTime > itr->startTimeMs) {
            // overlapped periods
            if (itr->durationMs == -1)
                last.durationMs = -1;
            else
                last.durationMs = qMax(last.durationMs, itr->endTimeMs() - last.startTimeMs);
        }
        else if (qAbs(lastEndTime - itr->startTimeMs) <= detailLevel && itr->durationMs != -1)
            last.durationMs = itr->startTimeMs - last.startTimeMs + itr->durationMs;
        else {
            if (last.durationMs < detailLevel && result.size() > 1 && !keepSmalChunks)
                result.pop_back();
            if (result.size() >= limit)
                break;
            result.push_back(QnTimePeriod(itr->startTimeMs, itr->durationMs));
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

    int timeZoneExist = 0;
    QByteArray headerLine = file.readLine();
    if (headerLine.contains("timezone"))
        timeZoneExist = 1;
    QByteArray line;
    //bool checkDirOnly = false;
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

        addChunk(chunk);

    } while (!line.isEmpty());

    return true;
}

QnRecordingStatsData DeviceFileCatalog::getStatistics(qint64 bitrateAnalizePeriodMs) const
{
    QnRecordingStatsData result;
    QnRecordingStatsData bitrateStats;
    QnMutexLocker lock(&m_mutex);

    if (m_chunks.empty())
        return QnRecordingStatsData();

    result.archiveDurationSecs = qMax(0ll, (qnSyncTime->currentMSecsSinceEpoch() -  m_chunks[0].startTimeMs) / 1000);

    auto itrLeft = m_chunks.cbegin();
    auto itrRight = m_chunks.cend();

    qint64 bitrateThreshold = bitrateAnalizePeriodMs ? m_chunks[m_chunks.size()-1].startTimeMs - bitrateAnalizePeriodMs : 0;

    for (auto itr = itrLeft; itr != itrRight; ++itr)
    {
        const Chunk& chunk = *itr;
        auto storage = getMyStorageMan()->storageRoot(chunk.storageIndex);
        if (chunk.durationMs != Chunk::UnknownDuration) {
            result.recordedBytes += chunk.getFileSize();
            if (storage)
                result.recordedBytesPerStorage[storage->getId()] += chunk.getFileSize();
            result.recordedSecs += chunk.durationMs;

            if (chunk.startTimeMs >= bitrateThreshold) {
                bitrateStats.recordedBytes += chunk.getFileSize();
                bitrateStats.recordedSecs += chunk.durationMs;
            }
        }
    }
    result.recordedSecs /= 1000;       // msec to sec
    bitrateStats.recordedSecs /= 1000; // msec to sec
    if (bitrateStats.recordedBytes > 0 && bitrateStats.recordedSecs > 0)
        result.averageBitrate = bitrateStats.recordedBytes / (qreal) bitrateStats.recordedSecs;
    NX_ASSERT(result.averageBitrate >= 0);
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

bool operator == (const DeviceFileCatalog::Chunk &lhs, const DeviceFileCatalog::Chunk &rhs)
{
    return lhs.startTimeMs == rhs.startTimeMs && lhs.durationMs == rhs.durationMs &&
           lhs.fileIndex == rhs.fileIndex && lhs.getFileSize() == rhs.getFileSize() &&
           lhs.timeZone == rhs.timeZone && lhs.storageIndex == rhs.storageIndex;
}

bool DeviceFileCatalog::ScanFilter::intersects(const QnTimePeriod& period) const {
    return QnTimePeriod(scanPeriod.startTimeMs, scanPeriod.durationMs).intersects(period);
}
