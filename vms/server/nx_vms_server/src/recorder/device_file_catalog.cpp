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
#include "core/resource/avi/avi_archive_delegate.h"
#include "recording/stream_recorder.h"
#include "core/resource/avi/avi_resource.h"
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
#include <nx/vms/server/resource/storage_resource.h>

QnMutex DeviceFileCatalog::m_rebuildMutex;
QSet<void*> DeviceFileCatalog::m_pauseList;


using namespace nx::vms::server;
using namespace std::chrono;

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

DeviceFileCatalog::DeviceFileCatalog(
    QnMediaServerModule* serverModule, const QString& cameraUniqueId,
    QnServer::ChunksCatalog catalog, QnServer::StoragePool storagePool)
    :
    nx::vms::server::ServerModuleAware(serverModule),
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
    replaceChunks(storageIndex, nx::vms::server::ChunksDeque());
}

void DeviceFileCatalog::replaceChunks(
    int storageIndex, const nx::vms::server::ChunksDeque& newCatalog)
{
    NX_ASSERT(std::is_sorted(newCatalog.begin(), newCatalog.end()));

    QnMutexLocker lock( &m_mutex );

    m_chunks.remove_if(
        [storageIndex](const auto& chunk){ return chunk.storageIndex == storageIndex; } );

    m_chunks.set_union(newCatalog.begin(), newCatalog.end());
}

QSet<QDate> DeviceFileCatalog::recordedMonthList()
{
    QnMutexLocker lock( &m_mutex );

    QSet<QDate> rez;
    if (m_chunks.empty())
        return rez;

    auto curItr = m_chunks.begin();
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
        auto itr = std::upper_bound(m_chunks.cbegin(), m_chunks.cend(), chunk.startTimeMs);
        m_chunks.insert(itr, chunk);
    }
    if (m_chunks.size() > 1 && m_chunks[m_chunks.size()-1].startTimeMs == m_chunks[m_chunks.size()-2].endTimeMs() + 1)
    {
        // update catalog to fix bug from previous version
        auto chunkToUpdate = m_chunks[m_chunks.size()-2];
        chunkToUpdate.startTimeMs =
            m_chunks[m_chunks.size()-1].startTimeMs - chunkToUpdate.startTimeMs;
        m_chunks.update(m_chunks.size()-2, chunkToUpdate);
        return true;
    }
    return false;
}

int64_t DeviceFileCatalog::occupiedSpace(int storageIndex) const
{
    QnMutexLocker lk( &m_mutex );
    return m_chunks.occupiedSpace(storageIndex);
}

milliseconds DeviceFileCatalog::occupiedDuration(int storageIndex) const
{
    QnMutexLocker lk( &m_mutex );
    return m_chunks.occupiedDuration(storageIndex);
}

milliseconds DeviceFileCatalog::calendarDuration(int storageIndex) const
{
    QnMutexLocker lk(&m_mutex);
    if (m_chunks.empty())
        return milliseconds(0);
    qint64 endTimeMs = m_chunks.rbegin()->endTimeMs();
    if (endTimeMs == DATETIME_NOW)
        endTimeMs = qnSyncTime->currentMSecsSinceEpoch();
    return milliseconds(endTimeMs - m_chunks.begin()->startTimeMs);
}

void DeviceFileCatalog::addChunks(const nx::vms::server::ChunksDeque& chunks)
{
    NX_ASSERT(std::is_sorted(chunks.begin(), chunks.end()));
    QnMutexLocker lk(&m_mutex);
    m_chunks.set_union(chunks.begin(), chunks.end());
}

void DeviceFileCatalog::addChunks(const std::deque<Chunk>& chunks)
{
    NX_ASSERT(std::is_sorted(chunks.begin(), chunks.end()));
    QnMutexLocker lk(&m_mutex);
    m_chunks.set_union(chunks.begin(), chunks.end());
}

std::deque<Chunk> DeviceFileCatalog::mergeChunks(
    const std::deque<Chunk>& chunks1, const std::deque<Chunk>& chunks2)
{
    std::deque<Chunk> result;
    result.resize(chunks1.size() + chunks2.size());
    std::merge(chunks1.begin(), chunks1.end(), chunks2.begin(), chunks2.end(), result.begin());
    return result;
}

int DeviceFileCatalog::detectTimeZone(qint64 startTimeMs, const QString& fileName)
{
    QDateTime datetime1 = QDateTime::fromMSecsSinceEpoch(startTimeMs).toUTC();
    datetime1 = datetime1.addMSecs(
        -(datetime1.time().minute()*60*1000ll
            + datetime1.time().second()*1000ll
            + datetime1.time().msec()));

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

nx::vms::server::Chunk DeviceFileCatalog::chunkFromFile(
    const QnStorageResourcePtr& storage, const QString& fileName)
{
    Chunk chunk;
    if (fileName.indexOf(lit(".txt")) != -1)
        return chunk;

    const QString localFileName = toLocalStoragePath(storage, fileName);

    if (QnFile::baseName(fileName).indexOf(lit("_")) == -1)
    {
        QnAviResourcePtr res(new QnAviResource(fileName, serverModule()->commonModule()));
        QnAviArchiveDelegate* avi = new QnAviArchiveDelegate();
        avi->setStorage(storage);
        avi->setFastStreamFind(true);
        if (avi->open(res, /*archiveIntegrityWatcher*/ nullptr) && avi->findStreams() && avi->endTime() != (qint64)AV_NOPTS_VALUE)
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
                storageDbPool()->getStorageIndex(storage),
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
        storageDbPool()->getStorageIndex(storage),
        fileIndex,
        durationMs,
        detectTimeZone(startTimeMs, localFileName)
    );
    return chunk;
}

QnStorageManager *DeviceFileCatalog::getMyStorageMan() const
{
    if (m_storagePool == QnServer::StoragePool::Normal)
        return serverModule()->normalStorageManager();
    else if (m_storagePool == QnServer::StoragePool::Backup)
        return serverModule()->backupStorageManager();
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

    static const int kMaxFolderDepth = 4;
    if (folders.size() > kMaxFolderDepth)
    {
        NX_WARNING(this, lm("Skip invalid folder %1 for scan media files.").arg(dirName));
        return QnTimePeriod(); //< Invalid folder structure.
    }

    QString timestamp(lit("%1/%2/%3T%4:00:00"));
    for (int i = 0; i < folders.size(); ++i)
    {
        bool ok = false;
        timestamp = timestamp.arg(folders[i].toInt(&ok), i == 0 ? 4 : 2, 10, QChar('0'));
        if (!ok)
        {
            NX_WARNING(this, lm("Skip invalid folder %1 for scan media files.").arg(dirName));
            return QnTimePeriod(); //< Invalid folder structure.
        }
    }
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

void DeviceFileCatalog::scanMediaFiles(
    const QString& folder, const QnStorageResourcePtr &storage, QMap<qint64, Chunk>& allChunks,
    QVector<EmptyFileInfo>& emptyFileList, const ScanFilter& filter)
{
    NX_VERBOSE(this, "%1 Processing directory %2", __func__, nx::utils::url::hidePassword(folder));
    QnAbstractStorageResource::FileInfoList files;

    for(const QnAbstractStorageResource::FileInfo& fi: storage->getFileList(folder))
    {
        while (!getMyStorageMan()->needToStopMediaScan() && needRebuildPause())
            QnLongRunnable::msleep(100);

        if (getMyStorageMan()->needToStopMediaScan() || serverModule()->commonModule()->isNeedToStop())
        {
            NX_VERBOSE(this, lit("%1 Stop requested. Cancelling").arg(Q_FUNC_INFO));
            return; // canceled
        }

        if (fi.isDir())
        {
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

    NX_DEBUG(this, "[Scan] started for directory: %1", nx::utils::url::hidePassword(folder));

    QThreadPool tp;
    tp.setMaxThreadCount(4);
    QnMutex scanFilesMutex;
    for(const auto& fi: files)
    {
        if (serverModule()->commonModule()->isNeedToStop())
        {
            NX_VERBOSE(this, lit("%1 Stop requested. Cancelling").arg(Q_FUNC_INFO));
            break;
        }

        nx::utils::concurrent::run(
            &tp,
            [&]()
            {
                //QString fileName = QDir::toNativeSeparators(fi.absoluteFilePath());
                QString fileName = fi.absoluteFilePath();
                qint64 fileTime = QFileInfo(fileName).baseName().toLongLong();

                if (!filter.isEmpty() && fileTime > filter.scanPeriod.endTimeMs())
                    return;

                Chunk chunk = chunkFromFile(storage, fileName);
                chunk.setFileSize(fi.size());
                if (!filter.isEmpty() && chunk.startTimeMs > filter.scanPeriod.endTimeMs())
                    return;

                QnMutexLocker lock(&scanFilesMutex);
                if (chunk.durationMs > 0 && chunk.startTimeMs > 0)
                {
                    QMap<qint64, Chunk>::iterator itr = allChunks.insert(chunk.startTimeMs, chunk);
                    if (itr != allChunks.begin())
                    {
                        Chunk& prevChunk = *(itr-1);
                        qint64 delta = chunk.startTimeMs - prevChunk.endTimeMs();
                        if (delta < MAX_FRAME_DURATION_MS
                            && !fi.baseName().contains("_")) //< An old version file.
                        {
                            prevChunk.durationMs = chunk.startTimeMs - prevChunk.startTimeMs;
                        }
                    }

                    if (allChunks.size() % 1000 == 0)
                        qWarning() << allChunks.size() << "media files processed...";

                }
                else if (fi.fileName().indexOf(".txt") == -1)
                {
                    qDebug()
                        << "remove file" << fi.absoluteFilePath()
                        << "because of empty chunk. duration=" << chunk.durationMs
                        << "startTime=" << chunk.startTimeMs;
                    emptyFileList << EmptyFileInfo(chunk.startTimeMs, fi.absoluteFilePath());
                }
            });
    }
    tp.waitForDone();

    NX_DEBUG(this, "[Scan] finished for directory: %1, %2 files processed",
        nx::utils::url::hidePassword(folder), allChunks.size());
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

QnServer::ChunksCatalog DeviceFileCatalog::getRole() const
{
    return m_catalog; // it is a const data
}

void DeviceFileCatalog::addRecord(const Chunk& chunk, bool sideRecorder)
{
    NX_ASSERT(chunk.durationMs < 1000 * 1000);

    QnMutexLocker lock( &m_mutex );

    auto itr = std::upper_bound(m_chunks.begin(), m_chunks.end(), chunk.startTimeMs);
    if (itr != m_chunks.end())
        m_chunks.insert(itr, chunk);  //triggers NX_ASSERT if itr == end()
    else
        m_chunks.push_back( chunk );

    if (!sideRecorder)
        m_recordingChunkTime = chunk.startTimeMs;
}

void DeviceFileCatalog::removeRecord(int idx)
{
    if (idx == 0)
        m_chunks.pop_front();
    else
        m_chunks.erase(m_chunks.begin() + idx);
}

Chunk DeviceFileCatalog::takeChunk(qint64 startTimeMs, qint64 durationMs) {
    QnMutexLocker lock( &m_mutex );

    auto itr = std::upper_bound(m_chunks.begin(), m_chunks.end(), startTimeMs);
    if (itr > m_chunks.begin())
        --itr;
    while (itr > m_chunks.begin() && itr->startTimeMs == startTimeMs && itr->durationMs != durationMs)
        --itr;

    if (itr != m_chunks.end() && itr->startTimeMs == startTimeMs && itr->durationMs == durationMs)
    {
        auto result = *itr;
        int idx = itr - m_chunks.begin();
        removeRecord(idx);
        return result;
    }

    return Chunk();
}

void DeviceFileCatalog::assignChunksUnsafe(
    std::deque<Chunk>::const_iterator begin,
    std::deque<Chunk>::const_iterator end)
{
    m_chunks.assign(begin, end);
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

Chunk DeviceFileCatalog::updateDuration(
    int durationMs, qint64 fileSize, bool indexWithDuration, qint64 startTimeMs)
{
    NX_ASSERT(durationMs < 1000 * 1000);
    QnMutexLocker lock( &m_mutex );
    //m_chunks.last().durationMs = durationMs;
    const auto timeToFind = startTimeMs == AV_NOPTS_VALUE
        ? m_recordingChunkTime
        : startTimeMs;

    auto itr = std::lower_bound(m_chunks.begin(), m_chunks.end(), timeToFind);
    if (itr != m_chunks.end() && itr->startTimeMs == timeToFind)
    {
        Chunk chunk = *itr;
        chunk.durationMs = durationMs;
        chunk.setFileSize(fileSize);
        if (indexWithDuration)
            chunk.fileIndex = Chunk::FILE_INDEX_WITH_DURATION;
        m_chunks.update(itr, chunk);
        return chunk;
    }
    else
    {
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

QVector<Chunk> DeviceFileCatalog::deleteRecordsBefore(int idx)
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

    for (int i = 0; i < m_chunks.size();)
    {
        if (m_chunks[i].storageIndex == storageIndex)
        {
            if (m_chunks[i].startTimeMs < timeMs)
                removeRecord(i);
            else
                break;
        }
        else
        {
            ++i;
        }
    }
}

bool DeviceFileCatalog::isEmpty() const
{
    QnMutexLocker lock( &m_mutex );
    return m_chunks.empty();
}

Chunk DeviceFileCatalog::deleteFirstRecord()
{
    StorageResourcePtr storage;
    QString delFileName;
    int storageIndex;
    Chunk deletedChunk;
    {
        QnMutexLocker lock( &m_mutex );

        if (m_chunks.empty())
        {
            return deletedChunk;
        }
        else
        {
            storageIndex = m_chunks[0].storageIndex;
            delFileName = fullFileName(m_chunks[0]);
            deletedChunk = m_chunks[0];
            removeRecord(0);
        }
    }

    storage = getMyStorageMan()->storageRoot(storageIndex);
    if (!storage || delFileName.isEmpty())
        return deletedChunk; //< Nothing to delete.
    setHasArchiveRotated(true);
    serverModule()->fileDeletor()->deleteFile(delFileName, storage->getId());
    return deletedChunk;
}

int DeviceFileCatalog::findNextFileIndex(qint64 startTimeMs) const
{
    QnMutexLocker lock( &m_mutex );
    if (m_chunks.empty())
        return -1;
    auto itr = std::upper_bound(m_chunks.begin(), m_chunks.end(), startTimeMs);
    return itr == m_chunks.end() ? -1 : itr - m_chunks.begin();
}

int DeviceFileCatalog::findFileIndex(qint64 startTimeMs, FindMethod method) const
{
    QnMutexLocker lock( &m_mutex );

    if (m_chunks.empty())
        return -1;

    auto itr = std::upper_bound(m_chunks.begin(), m_chunks.end(), startTimeMs);
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
    auto itr = std::lower_bound(m_chunks.begin(), m_chunks.end(), chunk.startTimeMs);
    if (itr != m_chunks.end() && itr->startTimeMs == chunk.startTimeMs)
        chunk.durationMs = itr->durationMs;
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

    auto itr = std::upper_bound(m_chunks.begin(), m_chunks.end(), timeMs);
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

    auto itr = std::lower_bound(m_chunks.begin(), m_chunks.end(), period.startTimeMs);
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

nx::vms::server::Chunk DeviceFileCatalog::chunkAt(int index) const
{
    QnMutexLocker lock( &m_mutex );
    if (index >= 0 && (size_t)index < m_chunks.size() )
        return m_chunks.at(index);
    else
        return Chunk();
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

QnTimePeriodList DeviceFileCatalog::getTimePeriods(
    qint64 startTimeMs,
    qint64 endTimeMs,
    qint64 detailLevel,
    bool keepSmalChunks,
    int limit,
    Qt::SortOrder sortOrder)
{
    QnMutexLocker lock( &m_mutex );
    QnTimePeriodList result;
    if (m_chunks.empty())
        return result;

    auto itr = std::lower_bound(m_chunks.begin(), m_chunks.end(), startTimeMs);
    /* Checking if we should include a chunk, containing startTime. */
    if (itr != m_chunks.begin())
    {
        --itr;
        if (itr->endTimeMs() <= startTimeMs)
            ++itr; //< Case if previous chunk does not contain startTime.
    }

    if (itr == m_chunks.end() || itr->startTimeMs >= endTimeMs)
        return result;

    auto endItr = std::lower_bound(m_chunks.begin(), m_chunks.end(), endTimeMs);
    return QnTimePeriodList::filterTimePeriods(
        itr, endItr, detailLevel, keepSmalChunks, limit, sortOrder);
}

bool DeviceFileCatalog::fromCSVFile(const QString& fileName)
{
    QFile file(fileName);

    if (!file.open(QFile::ReadOnly))
    {
        NX_ERROR(this, lit("Can't open title file %1").arg(file.fileName()));
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
        int coeff = 1; // compatibility with previous version (convert us to ms)
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

QnRecordingStatsData DeviceFileCatalog::getStatistics(qint64 bitrateAnalyzePeriodMs) const
{
    QnRecordingStatsData result;
    QnMutexLocker lock(&m_mutex);

    NX_VERBOSE(this, "getStatistics for camera %1, analize period %2, catalog size %3",
        m_cameraUniqueId, bitrateAnalyzePeriodMs, m_chunks.size());

    if (m_chunks.empty())
    {
        NX_VERBOSE(this,
            "getStatistics for camera %1. Device file catalog is empty", m_cameraUniqueId);
        return QnRecordingStatsData();
    }

    result.recordedBytes = occupiedSpace();
    result.recordedSecs = duration_cast<seconds>(occupiedDuration()).count();
    for (const auto&[index, data]: m_chunks.archivePresence())
    {
        if (auto storage = getMyStorageMan()->storageRoot(index))
            result.recordedBytesPerStorage[storage->getId()] = data.space;
    }

    result.archiveDurationSecs =
        qMax(
            0ll,
            (qnSyncTime->currentMSecsSinceEpoch() -  m_chunks.front().startTimeMs) / 1000);

    qint64 averagingStartTime = bitrateAnalyzePeriodMs != 0
        ? m_chunks.back().startTimeMs - bitrateAnalyzePeriodMs : 0;
    NX_VERBOSE(this, "getStatistics for camera %1. averagingStartTime %1", averagingStartTime);


    qint64 recordedMsForPeriod = 0;
    qint64 recordedBytesForPeriod = 0;

    for (auto itr = std::lower_bound(m_chunks.begin(), m_chunks.end(), averagingStartTime);
        itr != m_chunks.end(); ++itr)
    {
        const auto& chunk = *itr;
        if (chunk.durationMs != Chunk::UnknownDuration)
        {
            recordedBytesForPeriod += chunk.getFileSize();
            recordedMsForPeriod += chunk.durationMs;
        }
    }
    NX_VERBOSE(this,
        "getStatistics for camera %1. recordedBytesForPeriod %2, recordedMsForPeriod %3",
        m_cameraUniqueId, recordedBytesForPeriod, recordedMsForPeriod);

    if (recordedBytesForPeriod > 0)
    {
        qint64 averagingPeriodMs = bitrateAnalyzePeriodMs != 0 // bitrateAnalyzePeriodMs or all archive
            ? bitrateAnalyzePeriodMs
            : qMax(1ll, qnSyncTime->currentMSecsSinceEpoch() - m_chunks.front().startTimeMs);

        result.averageDensity = recordedBytesForPeriod / (qreal) averagingPeriodMs * 1000; // msec to sec
        if (recordedMsForPeriod > 0)
            result.averageBitrate = recordedBytesForPeriod / (qreal) recordedMsForPeriod * 1000; // msec to sec
    }

    NX_ASSERT(result.averageBitrate >= 0);
    return result;
}

bool DeviceFileCatalog::ScanFilter::intersects(const QnTimePeriod& period) const
{
    return QnTimePeriod(scanPeriod.startTimeMs, scanPeriod.durationMs).intersects(period);
}
