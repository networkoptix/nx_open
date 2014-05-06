#ifndef _DEVICE_FILE_CATALOG_H___
#define _DEVICE_FILE_CATALOG_H___

#include <QSharedPointer>
#include <QtCore/QFile>
#include <QtCore/QVector>
#include <QtCore/QMap>
#include "core/resource/resource.h"
#include "core/resource/network_resource.h"
#include "recording/time_period.h"
#include <QtCore/QFileInfo>

class DeviceFileCatalog: public QObject
{
    Q_OBJECT
signals:
    void firstDataRemoved(int n);
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

    // TODO: #Elric #enum
    enum FindMethod {OnRecordHole_NextChunk, OnRecordHole_PrevChunk};

    DeviceFileCatalog(const QString& macAddress, QnResource::ConnectionRole role);
    //void deserializeTitleFile();
    void addRecord(const Chunk& chunk);
    Chunk updateDuration(int durationMs, qint64 fileSize);

    /** return deleted file size if calcFileSize is true and srcStorage matched with deleted file */
    qint64 deleteFirstRecord(); 
    bool isEmpty() const;
    void clear();
    QVector<qint64> deleteRecordsBefore(int idx);
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
    QnResource::ConnectionRole getRole() const { return m_role; }
    QByteArray getMac() const { return m_macAddress.toUtf8(); }

    // Detail level determine time duration (in microseconds) visible at 1 screen pixel
    // All information less than detail level is discarded
    typedef QVector<Chunk> ChunkMap;

    QnTimePeriodList getTimePeriods(qint64 startTime, qint64 endTime, qint64 detailLevel);
    void close();

    static QString prefixForRole(QnResource::ConnectionRole role);
    static QnResource::ConnectionRole roleForPrefix(const QString& prefix);

    static void setRebuildArchive(RebuildMethod value);
    static void cancelRebuildArchive();
    static bool needRebuildPause();
    static void rebuildPause(void*);
    static void rebuildResume(void*);
    static QMutex m_rebuildMutex;
    static QSet<void*> m_pauseList;
    qint64 m_rebuildStartTime;

    //bool readCatalog();
    bool doRebuildArchive(QnStorageResourcePtr storage, const QnTimePeriod& period);
    //void rewriteCatalog(bool isCatalogUsing);
    bool isLastRecordRecording() const { return m_lastRecordRecording; }
    qint64 getLatRecordingTime() const;
    void setLatRecordingTime(qint64 value);

    struct ScanFilter
    {
        Chunk scanAfter;

        bool isEmpty() const { return scanAfter.durationMs == 0; }
        bool intersects(const QnTimePeriod& period) const { return QnTimePeriod(scanAfter.startTimeMs, scanAfter.durationMs).intersects(period); }
    };

    void scanMediaFiles(const QString& folder, QnStorageResourcePtr storage, QMap<qint64, Chunk>& allChunks, QStringList& emptyFileList,
        const ScanFilter& filter = ScanFilter());

    static QVector<Chunk> mergeChunks(const QVector<Chunk>& chunk1, const QVector<Chunk>& chunk2);
    void addChunks(const QVector<Chunk>& chunk);
    bool fromCSVFile(const QString& fileName);
private:
    bool fileExists(const Chunk& chunk, bool checkDirOnly);
    bool addChunk(const Chunk& chunk);
    qint64 recreateFile(const QString& fileName, qint64 startTimeMs, QnStorageResourcePtr storage);
    QList<QDate> recordedMonthList();

    void readStorageData(QnStorageResourcePtr storage, QnResource::ConnectionRole role, QMap<qint64, Chunk>& allChunks, QStringList& emptyFileList);
    Chunk chunkFromFile(QnStorageResourcePtr storage, const QString& fileName);
    QnTimePeriod timePeriodFromDir(QnStorageResourcePtr storage, const QString& dirName);
    void replaceChunks(int storageIndex, const QVector<Chunk>& newCatalog);
private:
    friend class QnStorageManager;

    mutable QMutex m_mutex;
    //QFile m_file;
    QVector<Chunk> m_chunks; 
    int m_firstDeleteCount;
    QString m_macAddress;

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
    QnResource::ConnectionRole m_role;
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
