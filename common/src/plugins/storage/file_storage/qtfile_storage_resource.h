#ifndef _QTFILE_STORAGE_PROTOCOL_H__
#define _QTFILE_STORAGE_PROTOCOL_H__

#ifdef ENABLE_DATA_PROVIDERS

extern "C"
{
    #include <libavformat/avio.h>
}

#include "core/resource/storage_resource.h"

/*
* QnQtFileStorageResource uses Qt based IO access
*/

class QnQtFileStorageResource: public QnStorageResource
{
    using base_type = QnStorageResource;
public:
    QnQtFileStorageResource(QnCommonModule* commonModule);

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

protected:
private:
    QString removeProtocolPrefix(const QString& url) const;
private:
    int  m_capabilities; // see QnAbstractStorageResource::cap flags
};

#endif //ENABLE_DATA_PROVIDERS

#endif // _FILE_STORAGE_PROTOCOL_H__
