#ifndef __THIRD_PARTY_STORAGE_RESOURCE_H__
#define __THIRD_PARTY_STORAGE_RESOURCE_H__

#include <memory>

#include "core/resource/storage_resource.h"
#include "utils/library/library.h"
#include "plugins/storage/third_party/third_party_storage.h"

class QnThirdPartyStorageResource
    : public QnStorageResource
{
public: //typedefs
    typedef std::shared_ptr<
        Qn::Storage
    > StoragePtrType;

    typedef std::shared_ptr<
        Qn::FileInfoIterator
    > FileInfoIteratorPtrType;

    typedef std::shared_ptr<
        Qn::IODevice
    > IODevicePtrType;

public: // static funcs
    static QnStorageResource* instance(const QString&);

public: //ctors, dtor
    QnThirdPartyStorageResource(
        const QString  &libraryPath,
        const QString  &storageUrl
    );

    QnThirdPartyStorageResource(); //designates invalid storagere source

    ~QnThirdPartyStorageResource();

public: // inherited interface overrides
    virtual QIODevice *open(
        const QString       &fileName, 
        QIODevice::OpenMode  openMode
    ) override;

    virtual int     getCapabilities() const override;    
    virtual qint64  getFreeSpace() override;
    virtual qint64  getTotalSpace() override;
    virtual bool    isAvailable() const override;
    virtual bool    removeFile(const QString& url) override;
    virtual bool    removeDir(const QString& url) override;
    virtual bool    renameFile(const QString& oldName, const QString& newName) override;
    virtual bool    isFileExists(const QString& url) override;
    virtual bool    isDirExists(const QString& url) override;
    virtual qint64  getFileSize(const QString& url) const override;

    virtual 
    QnAbstractStorageResource::FileInfoList 
    getFileList(const QString& dirName) override;

private:
    void openStorage(const char *storageUrl);
private:
    QnLibrary                           m_lib;
    create_qn_storage_factory_function  m_csf;
    qn_storage_error_message_function   m_emf;
    StoragePtrType                      m_storage;
    mutable QMutex                      m_mutex;
    bool                                m_valid;
}; // QnThirdPartyStorageResource

#endif