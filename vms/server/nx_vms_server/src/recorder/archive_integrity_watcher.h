#pragma once

#include <atomic>
#include <nx/streaming/abstract_archive_delegate.h>
#include <nx/vms/server/server_module_aware.h>


namespace nx {
namespace vms::server {

class IntegrityHashHelper
{
public:
    static QByteArray generateIntegrityHash(const QByteArray& value);
    static bool checkIntegrity(const QByteArray& initialValue, const QByteArray& hashedValue);
private:
    static const QByteArray kIntegrityHashSalt;

    static QByteArray hashWithSalt(const QByteArray& value);
};

class ServerArchiveIntegrityWatcher: 
    public QObject, 
    public AbstractArchiveIntegrityWatcher,
    public ServerModuleAware
{
    Q_OBJECT

public:
    ServerArchiveIntegrityWatcher(QnMediaServerModule* serverModule);

    virtual bool fileRequested(
        const QnAviArchiveMetadata& metadata,
        const QString& fileName) override;
    virtual void fileMissing(const QString& fileName) override;
    virtual void reset() override;

signals:
    void fileIntegrityCheckFailed(const QnStorageResourcePtr& storage);

private:
    std::atomic<bool> m_fired{false};

    bool checkMetaDataIntegrity(const QnAviArchiveMetadata& metadata);
    bool checkFileName(const QnAviArchiveMetadata& metadata, const QString& fileName);
    void emitSignal(const QString& fileName);
};

} // namespace vms::server
} // namespace nx
