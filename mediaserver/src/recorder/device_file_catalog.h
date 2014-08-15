#ifndef _DEVICE_FILE_CATALOG_H___
#define _DEVICE_FILE_CATALOG_H___

#include <QSharedPointer>
#include <QtCore/QFile>
#include <QtCore/QVector>
#include <QtCore/QMap>
#include <deque>

#include <server/server_globals.h>

#include "core/resource/resource.h"
#include "core/resource/network_resource.h"
#include <QtCore/QFileInfo>

class QnTimePeriodList;
class QnTimePeriod;

class DeviceFileCatalog: public QObject
{
    Q_OBJECT
public:
    // TODO: #Elric #enum
    enum RebuildMethod {
        Rebuild_None,    // do not rebuild chunk's database
        Rebuild_Canceled,
        Rebuild_LQ,      // rebuild LQ chunks only
        Rebuild_HQ,      // rebuild HQ chunks only
        Rebuild_All      // rebuild whole chunks
    };

    struct Chunk
    {
        Chunk(): startTimeMs(-1), durationMs(0), storageIndex(0), fileIndex(0),timeZone(-1) {}
        Chunk(qint64 _startTime, int _storageIndex, int _fileIndex, int _duration, qint16 _timeZone, quint16 fileSizeHi = 0, quint32 fileSizeLo = 0) : 
            startTimeMs(_startTime), durationMs(_duration), storageIndex(_storageIndex), fileIndex(_fileIndex), timeZone(_timeZone), fileSizeHi(fileSizeHi), fileSizeLo(fileSizeLo)
        {
            Q_ASSERT_X(startTimeMs == -1 || startTimeMs > 0, Q_FUNC_INFO, "Invalid startTime value");
        }

        qint64 distanceToTime(qint64 timeMs) const;
        qint64 endTimeMs() const;
        bool containsTime(qint64 timeMs) const;
        void truncate(qint64 timeMs);
        qint64 getFileSize() const { return ((qint64) fileSizeHi << 32) + fileSizeLo; } // 256Tb as max file size
        void setFileSize(qint64 value) { fileSizeHi = quint16(value >> 32); fileSizeLo = quint32(value); } // 256Tb as max file size


        qint64 startTimeMs; // chunk startTime at ms
        int durationMs; // chunk duration at ms
        quint16 storageIndex;
        quint16 fileIndex;

        qint16 timeZone;
        quint16 fileSizeHi;
        quint32 fileSizeLo;
    };

    struct EmptyFileInfo
    {
        EmptyFileInfo(): startTimeMs(0) {}
        EmptyFileInfo(qint64 startTimeMs, const QString& fileName): startTimeMs(startTimeMs), fileName(fileName) {}

        qint64 startTimeMs;
        QString fileName;
    };

    // TODO: #Elric #enum
    enum FindMethod {OnRecordHole_NextChunk, OnRecordHole_PrevChunk};

    DeviceFileCatalog(const QString &cameraUniqueId, QnServer::ChunksCatalog catalog);
    //void deserializeTitleFile();
    void addRecord(const Chunk& chunk);
    Chunk updateDuration(int durationMs, qint64 fileSize);
    Chunk takeChunk(qint64 startTimeMs, qint64 durationMs);

    Chunk deleteFirstRecord(); 

    /** Delete first N records. Return deleted file timestamps */
    QVector<Chunk> deleteRecordsBefore(int idx);

    bool isEmpty() const;
    void clear();
    void deleteRecordsByStorage(int storageIndex, qint64 timeMs);
    int findFileIndex(qint64 startTimeMs, FindMethod method) const;
    void updateChunkDuration(Chunk& chunk);
    QString fullFileName(const Chunk& chunk) const;
    Chunk chunkAt(int index) const;
    bool isLastChunk(qint64 startTimeMs) const;
    bool containTime(qint64 timeMs, qint64 eps = 5 * 1000) const;

    qint64 minTime() const;
    qint64 maxTime() const;
    //bool lastFileDuplicateName() const;
    qint64 firstTime() const;
    QnServer::ChunksCatalog getCatalog() const { return m_catalog; }
    //QByteArray getMac() const { return m_macAddress.toUtf8(); }

    // Detail level determine time duration (in microseconds) visible at 1 screen pixel
    // All information less than detail level is discarded
    typedef std::deque<Chunk> ChunkMap;

    QnTimePeriodList getTimePeriods(qint64 startTime, qint64 endTime, qint64 detailLevel);
    void close();

    QString rootFolder(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog) const;
    QString cameraUniqueId() const;

    static QString prefixByCatalog(QnServer::ChunksCatalog catalog);
    static QnServer::ChunksCatalog catalogByPrefix(const QString &prefix);

    static void setRebuildArchive(RebuildMethod value);
    static void cancelRebuildArchive();
    static bool needRebuildPause();
    static void rebuildPause(void*);
    static void rebuildResume(void*);
    static QMutex m_rebuildMutex;
    static QSet<void*> m_pauseList;
    qint64 m_rebuildStartTime;

    bool doRebuildArchive(const QnStorageResourcePtr &storage, const QnTimePeriod& period);
    bool isLastRecordRecording() const { return m_lastRecordRecording; }
    qint64 getLatRecordingTime() const;
    void setLatRecordingTime(qint64 value);

    struct ScanFilter
    {
        Chunk scanAfter;

        bool isEmpty() const { return scanAfter.durationMs == 0; }
        bool intersects(const QnTimePeriod& period) const;
    };

    void scanMediaFiles(const QString& folder, const QnStorageResourcePtr &storage, QMap<qint64, Chunk>& allChunks, QVector<EmptyFileInfo>& emptyFileList,
        const ScanFilter& filter = ScanFilter());

    static std::deque<Chunk> mergeChunks(const std::deque<Chunk>& chunk1, const std::deque<Chunk>& chunk2);
    void addChunks(const std::deque<Chunk>& chunk);
    bool fromCSVFile(const QString& fileName);
    QnServer::ChunksCatalog getRole() const;
private:

    bool fileExists(const Chunk& chunk, bool checkDirOnly);
    bool addChunk(const Chunk& chunk);
    qint64 recreateFile(const QString& fileName, qint64 startTimeMs, const QnStorageResourcePtr &storage);
    QSet<QDate> recordedMonthList();

    void readStorageData(const QnStorageResourcePtr &storage, QnServer::ChunksCatalog catalog, QMap<qint64, Chunk>& allChunks, QVector<EmptyFileInfo>& emptyFileList);
    Chunk chunkFromFile(const QnStorageResourcePtr &storage, const QString& fileName);
    QnTimePeriod timePeriodFromDir(const QnStorageResourcePtr &storage, const QString& dirName);
    void replaceChunks(int storageIndex, const std::deque<Chunk>& newCatalog);
    void removeRecord(int idx);
private:
    friend class QnStorageManager;

    mutable QMutex m_mutex;
    //QFile m_file;
    std::deque<Chunk> m_chunks; 
    QString m_cameraUniqueId;

    typedef QVector<QPair<int, bool> > CachedDirInfo;
    struct IOCacheEntry
    {
        CachedDirInfo dirInfo;
        QFileInfoList entryList;

        IOCacheEntry() { dirInfo.resize(4); }
    };

    typedef QMap<int, IOCacheEntry > IOCacheMap;
    IOCacheMap m_prevPartsMap[4];

    //bool m_duplicateName;
    //QMap<int,QString> m_prevFileNames;
    const QnServer::ChunksCatalog m_catalog;
    int m_lastAddIndex; // last added record index. In most cases it is last record
    QMutex m_IOMutex;
    static RebuildMethod m_rebuildArchive;
    bool m_lastRecordRecording;
};

typedef QSharedPointer<DeviceFileCatalog> DeviceFileCatalogPtr;

bool operator < (const DeviceFileCatalog::Chunk& first, const DeviceFileCatalog::Chunk& other);
bool operator < (qint64 first, const DeviceFileCatalog::Chunk& other);
bool operator < (const DeviceFileCatalog::Chunk& other, qint64 first);

#endif // _DEVICE_FILE_CATALOG_H__
