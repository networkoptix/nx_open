#pragma once

#include "core/resource/storage_resource.h"
#include <storage/third_party_storage.h>
#include <nx/utils/thread/mutex.h>
#include <nx/sdk/ptr.h>
#include <nx/vms/server/resource/storage_resource.h>

namespace nx::vms::server { class Settings; }

class QnThirdPartyStorageResource: public nx::vms::server::StorageResource
{
    using base_type = nx::vms::server::StorageResource;
public:
    static QnStorageResource* instance(
        QnMediaServerModule* serverModule,
        const QString& url,
        nx_spl::StorageFactory* sf,
        const nx::vms::server::Settings* settings
    );

    QnThirdPartyStorageResource(
        QnMediaServerModule* serverModule,
        nx_spl::StorageFactory* sf,
        const QString& storageUrl,
        const nx::vms::server::Settings* settings
    );

    QnThirdPartyStorageResource(); //< Designates invalid storage resource.

    ~QnThirdPartyStorageResource() = default;

public: // Inherited interface overrides.
    virtual int getCapabilities() const override;
    virtual qint64 getFreeSpace() override;
    virtual qint64 getTotalSpace()const override;
    virtual Qn::StorageInitResult initOrUpdate() override;
    virtual bool removeDir(const QString& url) override;
    virtual bool renameFile(const QString& oldName, const QString& newName) override;
    virtual bool isFileExists(const QString& url) override;
    virtual bool isDirExists(const QString& url) override;
    virtual qint64 getFileSize(const QString& url) const override;
protected:
    virtual QIODevice* openInternal(const QString& fileName, QIODevice::OpenMode openMode) override;
private:
    void openStorage(const char* storageUrl, nx_spl::StorageFactory* storageFactory);

private:
    nx::sdk::Ptr<nx_spl::Storage> m_storage;
    mutable QnMutex m_mutex;
    bool m_valid;
    const nx::vms::server::Settings* m_settings = nullptr;

    virtual bool doRemoveFile(const QString& url) override;
    virtual QnAbstractStorageResource::FileInfoList doGetFileList(const QString& dirName) override;
};
