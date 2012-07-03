#ifndef _LAYOUT_STORAGE_PROTOCOL_H__
#define _LAYOUT_STORAGE_PROTOCOL_H__

#include <QFile>

#include <libavformat/avio.h>
#include "core/resource/storage_resource.h"

/*
* QnLayoutFileStorageResource uses for layout export
*/

class QnLayoutFile;

class QnLayoutFileStorageResource: public QnStorageResource
{
public:
    QnLayoutFileStorageResource();

    static QnStorageResource* instance();

    //void registerFfmpegProtocol() const override;
    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) override;

    virtual int getChunkLen() const override;
    virtual bool isStorageAvailable() override;
    virtual bool isStorageAvailableForWriting() override;
    virtual QFileInfoList getFileList(const QString& dirName) override;
    virtual qint64 getFileSize(const QString& fillName) const override;
    virtual bool isNeedControlFreeSpace() override;
    virtual bool removeFile(const QString& url) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;
    virtual qint64 getFreeSpace() override;

protected:

private:
    static QString removeProtocolPrefix(const QString& url);

    bool addFileEntry(const QString& fileName);
    qint64 getFileOffset(const QString& fileName, qint64* fileSize);
    void readIndexHeader();
private:
    static const int MAX_FILES_AT_LAYOUT = 256;
    static const quint64 MAGIC_STATIC = 0xfed8260da9eebc04ll;

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

    friend class QnLayoutFile;
    QnLayoutFileIndex m_index;
};

#endif // _LAYOUT_STORAGE_PROTOCOL_H__
