#ifndef _QTFILE_STORAGE_PROTOCOL_H__
#define _QTFILE_STORAGE_PROTOCOL_H__

#include <libavformat/avio.h>
#include "core/resource/storage_resource.h"

/*
* QnQtFileStorageResource uses Qt based IO access
*/

class QnQtFileStorageResource: public QnStorageResource
{
public:
    QnQtFileStorageResource();

    static QnStorageResource* instance();

    //void registerFfmpegProtocol() const override;
    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) override;

    virtual int getChunkLen() const override;
    virtual bool isStorageAvailable() override;
    virtual bool isStorageAvailableForWriting() override;
    virtual QFileInfoList getFileList(const QString& dirName) override;
    qint64 getFileSize(const QString& fillName) const override;
    virtual bool isNeedControlFreeSpace() override;
    virtual bool removeFile(const QString& url) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;
    virtual qint64 getFreeSpace() override;

protected:
private:
    QString removeProtocolPrefix(const QString& url);
};

#endif // _FILE_STORAGE_PROTOCOL_H__
