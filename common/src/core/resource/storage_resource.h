#ifndef QN_STORAGE_RESOURCE_H
#define QN_STORAGE_RESOURCE_H

#include <QSet>
#include "resource.h"
#include <QFileInfoList>
#include <QIODevice>

class QnAbstractMediaStreamDataProvider;

class QnAbstractStorageResource : public QnResource
{
    Q_OBJECT

    Q_PROPERTY(qint64 spaceLimit READ getSpaceLimit WRITE setSpaceLimit)
    Q_PROPERTY(int maxStoreTime READ getMaxStoreTime WRITE setMaxStoreTime)

public:
    QnAbstractStorageResource();
    virtual ~QnAbstractStorageResource();

    virtual QString getUniqueId() const;


    void setSpaceLimit(qint64 value);
    qint64 getSpaceLimit() const;

    void setMaxStoreTime(int timeInSeconds);
    int getMaxStoreTime() const;

    float bitrate() const;
    void addBitrate(QnAbstractMediaStreamDataProvider* provider);
    void releaseBitrate(QnAbstractMediaStreamDataProvider* provider);

    /*
     * Short and uniq storage ID. It is addition related ID field, and used for memory usage optimization
     */
    void setIndex(quint16 value);
    quint16 getIndex() const;

    void deserialize(const QnResourceParameters& parameters);
signals:
    /*
     * Storage may emit archiveRangeChanged signal to inform server what some data in archive already deleted
     * @param newStartTime - new archive start time point
     * @param newEndTime - Not used now, reserved for future use
     */
    void archiveRangeChanged(qint64 newStartTimeMs, qint64 newEndTimeMs);
private:
    qint64 m_spaceLimit;
    int m_maxStoreTime; // at seconds
    quint16 m_index;
    QSet<QnAbstractMediaStreamDataProvider*> m_providers;
};


class QnStorageResource : public QnAbstractStorageResource
{
    Q_OBJECT

public:
    QnStorageResource();
    virtual ~QnStorageResource();

    AVIOContext* createFfmpegIOContext(const QString& url, QIODevice::OpenMode openMode, int IO_BLOCK_SIZE = 32768);
    void closeFfmpegIOContext(AVIOContext* ioContext);
public:
    //virtual void registerFfmpegProtocol() const = 0;
    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) = 0;


    virtual int getChunkLen() const = 0;

    /*
    *   If function returns true, auto delete old files
    */
    virtual bool isNeedControlFreeSpace() = 0;

    /*
    *   Return storage free space. if isNeedControlFreeSpace returns false, this function is not used
    */
    virtual qint64 getFreeSpace() = 0;

    /*
    *   Returns true if storage physically acccesible right now (ready only check)
    */
    virtual bool isStorageAvailable() = 0;

    /*
    *   Returns true if storage physically acccesible and ready for writing
    */
    virtual bool isStorageAvailableForWriting() = 0;


    /*
    *   Remove file from storage
    */
    virtual bool removeFile(const QString& url) = 0;

    /*
    *   Remove dir from storage
    */
    virtual bool removeDir(const QString& url) = 0;

    /*
    *   This function is used when server restarts. Unfinished files re-readed, writed again (under a new name), then renamed.
    */
    virtual bool renameFile(const QString& oldName, const QString& newName) = 0;

    /*
    *   Returns file list in current directory, only files (but not subdirs) requires.
    *   QFileInfo structure MUST contains valid fields for full file name and file size.
    */
    virtual QFileInfoList getFileList(const QString& dirName) = 0;

    /*
    *   Returns true if file exists
    */
    virtual bool isFileExists(const QString& url) = 0;

    /*
    *   Returns true if dir exists
    */
    virtual bool isDirExists(const QString& url) = 0;

    virtual bool isRealFiles() const{return true;};

public:
    virtual void setUrl(const QString& value);
protected:

};

typedef QnStorageResource* (*StorageTypeInstance)();

class QnStoragePluginFactory
{
public:
    QnStoragePluginFactory();
    virtual ~QnStoragePluginFactory();
    static QnStoragePluginFactory* instance();

    void registerStoragePlugin(const QString& name, StorageTypeInstance pluginInst, bool isDefaultProtocol = false);
    QnStorageResource* createStorage(const QString& storageType, bool useDefaultForUnknownPrefix = true);
private:
    QMap<QString, StorageTypeInstance> m_storageTypes;
    StorageTypeInstance m_defaultStoragePlugin;
    QMutex m_mutex;
};

#endif // QN_STORAGE_RESOURCE_H
