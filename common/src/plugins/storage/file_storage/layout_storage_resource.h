#ifndef _LAYOUT_STORAGE_PROTOCOL_H__
#define _LAYOUT_STORAGE_PROTOCOL_H__

#include <QtCore/QFile>

extern "C"
{
    #include <libavformat/avio.h>
}
#include "core/resource/storage_resource.h"

class QnLayoutFile;
class QnTimePeriodList;

/*
* QnLayoutFileStorageResource uses for layout export
*/
class QnLayoutFileStorageResource: public QnStorageResource
{
    using base_type = QnStorageResource;
public:
    enum StorageFlags {
        ReadOnly        = 0x1,
        ContainsCameras = 0x2,
    };

    QnLayoutFileStorageResource(QnCommonModule* commonModule);
    virtual ~QnLayoutFileStorageResource();

    static QnStorageResource* instance(QnCommonModule* commonModule, const QString&);

    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) override;

    virtual int getCapabilities() const override;
    virtual Qn::StorageInitResult initOrUpdate() override;
    virtual QnAbstractStorageResource::FileInfoList getFileList(const QString& dirName) override;
    qint64 getFileSize(const QString& url) const override;
    virtual bool removeFile(const QString& url) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;
    virtual qint64 getFreeSpace() override;
    virtual qint64 getTotalSpace() const override;
    virtual void setUrl(const QString& value) override;

    bool switchToFile(const QString& oldName, const QString& newName, bool dataInOldFile);

    QnTimePeriodList getTimePeriods(const QnResourcePtr &resource);

    static QString itemUniqueId(const QString& layoutUrl, const QString& itemUniqueId);

    virtual QString getPath() const override;
public:
    static const int MAX_FILES_AT_LAYOUT = 256;

#pragma pack(push, 4)
    struct QnLayoutFileIndexEntry
    {
        QnLayoutFileIndexEntry(): offset(0), fileNameCrc(0), reserved(0) {}
        QnLayoutFileIndexEntry(qint64 _offset, quint32 _crc): offset(_offset), fileNameCrc(_crc), reserved(0) {}

        qint64 offset;
        quint32 fileNameCrc;
        quint32 reserved;
    };

    struct QnLayoutFileIndex
    {
        QnLayoutFileIndex(): magic(MAGIC_STATIC), version(1), entryCount(0) {}

        qint64 magic;
        quint32 version;
        quint32 entryCount;
        QnLayoutFileIndexEntry entries[MAX_FILES_AT_LAYOUT];
    };
#pragma pack(pop)

private:
    bool addFileEntry(const QString& fileName);
    qint64 getFileOffset(const QString& fileName, qint64* fileSize);
    bool readIndexHeader();
    void registerFile(QnLayoutFile* file);
    void unregisterFile(QnLayoutFile* file);

    void closeOpenedFiles();
    void restoreOpenedFiles();
    void addBinaryPostfix(QFile& file);
    int getPostfixSize() const;

    static const quint64 MAGIC_STATIC = 0xfed8260da9eebc04ll;

private:
    friend class QnLayoutFile;
    QnLayoutFileIndex m_index;
    QSet<QnLayoutFile*> m_openedFiles;
    QnMutex m_fileSync;
    static QnMutex m_storageSync;
    static QSet<QnLayoutFileStorageResource*> m_allStorages;
    qint64 m_novFileOffset;

    int m_capabilities;
    //qint64 m_novFileLen;
};

typedef QSharedPointer<QnLayoutFileStorageResource> QnLayoutFileStorageResourcePtr;

#endif // _LAYOUT_STORAGE_PROTOCOL_H__
