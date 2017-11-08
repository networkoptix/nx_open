#pragma once

#include <core/resource/storage_resource.h>
#include <api/app_server_connection.h>
#include <nx/utils/thread/wait_condition.h>

class QnDbStorageResource: public QnStorageResource
{
    using base_type = QnStorageResource;
Q_OBJECT
    enum class QnDbStorageResourceState
    {
        Waiting,
        Ready
    };

public:
    QnDbStorageResource(QnCommonModule* commonModule);
    ~QnDbStorageResource();

    static QnStorageResource* instance(QnCommonModule* commonModule, const QString&);

    virtual QIODevice* open(const QString& fileName, QIODevice::OpenMode openMode) override;
    virtual int getCapabilities() const override;
    virtual Qn::StorageInitResult initOrUpdate() override {return Qn::StorageInit_Ok;}
    virtual QnAbstractStorageResource::FileInfoList getFileList(const QString& /*dirName*/) override
    {return QnAbstractStorageResource::FileInfoList();}
    qint64 getFileSize(const QString& /*url*/) const override {return 0;}
    virtual bool removeFile(const QString& /*url*/) override {return false;}
    virtual bool removeDir(const QString& /*url*/) override {return false;}
    virtual bool renameFile(const QString& /*oldName*/, const QString& /*newName*/) override {return false;}
    virtual bool isFileExists(const QString& /*url*/) override {return true;}
    virtual bool isDirExists(const QString& /*url*/) override {return true;}
    virtual qint64 getFreeSpace() override {return 999999999;}
    virtual qint64 getTotalSpace() const override {return 9999999999;}

private:
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_cond;
    QnDbStorageResourceState m_state;
    QString m_filePath;
    QByteArray m_fileData;
    int m_capabilities;

private:
    QString removeProtocolPrefix(const QString& url) const;
};

