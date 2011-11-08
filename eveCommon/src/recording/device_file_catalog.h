#ifndef _DEVICE_FILE_CATALOG_H__
#define _DEVICE_FILE_CATALOG_H__

#include <QSharedPointer>
#include <QFile>
#include <QVector>
#include <QMap>
#include <QSharedPointer>
#include "core/resource/resource.h"

class DeviceFileCatalog: public QObject
{
    Q_OBJECT
signals:
    void firstDataRemoved(int n);
public:
    struct Chunk
    {
        Chunk(): startTime(-1), storageIndex(0), fileIndex(0), duration(0) {}
        Chunk(qint64 _startTime, int _storageIndex, int _fileIndex, int _duration) : 
            startTime(_startTime), storageIndex(_storageIndex), fileIndex(_fileIndex), duration(_duration) {}

        qint64 startTime;
        quint16 storageIndex;
        quint16 fileIndex;
        int duration; // chunk duration at ms
    };

    DeviceFileCatalog(QnResourcePtr resource);
    void deserializeTitleFile();
    void addRecord(const Chunk& chunk);
    void updateDuration(int duration);
    bool deleteFirstRecord();
    int findFileIndex(qint64 startTime) const;
    QString fullFileName(const Chunk& chunk) const;
    Chunk chunkAt(int index) const;
    qint64 minTime() const;
    qint64 maxTime() const;
    bool fileExists(const Chunk& chunk);
    bool lastFileDuplicateName() const;
    qint64 firstTime() const;
private:
    QString baseRoot(QnResourcePtr resource);
    qint64 getFileDuration(const QString& fileName);
private:
    mutable QMutex m_mutex;
    QFile m_file;
    QVector<Chunk> m_chunks; // key = chunk startTime at ms
    int m_firstDeleteCount;
    QnResourcePtr m_resource;

    QPair<int, bool> m_prevParts[4];
    QStringList m_existFileList;
    bool m_duplicateName;
    QString m_prevFileName;
};

typedef QVector<DeviceFileCatalog::Chunk> ChunkMap;
typedef QSharedPointer<DeviceFileCatalog> DeviceFileCatalogPtr;

bool operator < (const DeviceFileCatalog::Chunk& first, const DeviceFileCatalog::Chunk& other);
bool operator < (qint64 first, const DeviceFileCatalog::Chunk& other);
bool operator < (const DeviceFileCatalog::Chunk& other, qint64 first);

#endif // _DEVICE_FILE_CATALOG_H__
