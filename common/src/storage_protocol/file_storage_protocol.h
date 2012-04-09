#ifndef _FILE_STORAGE_PROTOCOL_H__
#define _FILE_STORAGE_PROTOCOL_H__

#include "abstract_storage_protocol.h"
#include <libavformat/avio.h>

class QnFileStorageProtocol: public QnAbstractStorageProtocol
{
public:
    QnFileStorageProtocol();

    virtual URLProtocol getURLProtocol() const override;

    virtual bool isStorageAvailable(const QString& value) override;
    virtual QFileInfoList getFileList(const QString& dirName) override;
    virtual bool isNeedControlFreeSpace() override;
    virtual bool removeFile(const QString& url) override;
    virtual bool removeDir(const QString& url) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;
    virtual qint64 getFreeSpace(const QString& url) override;
};

#endif // _FILE_STORAGE_PROTOCOL_H__
