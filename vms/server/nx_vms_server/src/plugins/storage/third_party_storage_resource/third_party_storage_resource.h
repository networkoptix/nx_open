#pragma once

#include <memory>

#include "core/resource/storage_resource.h"
#include <storage/third_party_storage.h>
#include <nx/utils/thread/mutex.h>
#include <nx/sdk/helpers/ptr.h>

namespace nx::vms::server { class Settings; }

class QnThirdPartyStorageResource: public QnStorageResource
{
    using base_type = QnStorageResource;
public: //typedefs
    typedef nx::sdk::Ptr<
        nx_spl::Storage
    > StoragePtrType;

    typedef nx_spl::StorageFactory *StorageFactoryPtrType;


    typedef nx::sdk::Ptr<
        nx_spl::FileInfoIterator
    > FileInfoIteratorPtrType;

    typedef nx::sdk::Ptr<
        nx_spl::IODevice
    > IODevicePtrType;

public: // static funcs
    static QnStorageResource* instance(
        QnCommonModule* commonModule,
        const QString               &url,
        const StorageFactoryPtrType &sf,
        const nx::vms::server::Settings* settings
    );

public: //ctors, dtor
    QnThirdPartyStorageResource(
        QnCommonModule* commonModule,
        const StorageFactoryPtrType &sf,
        const QString               &storageUrl,
        const nx::vms::server::Settings* settings
    );

    QnThirdPartyStorageResource(); //designates invalid storagere source


    ~QnThirdPartyStorageResource() = default;

public: // inherited interface overrides
    virtual QIODevice *open(
        const QString       &fileName,
        QIODevice::OpenMode  openMode
    ) override;

    virtual int     getCapabilities() const override;
    virtual qint64  getFreeSpace() override;
    virtual qint64  getTotalSpace()const override;
    virtual Qn::StorageInitResult initOrUpdate() override;
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
    void openStorage(
        const char                  *storageUrl,
        const StorageFactoryPtrType &storageFactory
    );
private:
    StoragePtrType                      m_storage;
    mutable QnMutex                      m_mutex;
    bool                                m_valid;
    const nx::vms::server::Settings* m_settings = nullptr;
}; // QnThirdPartyStorageResource
