#pragma once

#include <nx/streaming/abstract_archive_delegate.h>


namespace nx {
namespace mediaserver {

class IntegrityHashHelper
{
public:
    static QByteArray generateIntegrityHash(const QByteArray& value);
    static bool checkIntegrity(const QByteArray& initialValue, const QByteArray& hashedValue);
private:
    static const QByteArray kIntegrityHashSalt;

    static QByteArray hashWithSalt(const QByteArray& value);
};

class ServerArchiveIntegrityWatcher: public QObject, public AbstractArchiveIntegrityWatcher
{
    Q_OBJECT

public:
    virtual bool fileRequested(
        const QnAviArchiveMetadata& metadata,
        const QString& fileName) override;
    virtual void fileMissing(const QString& fileName) override;

signals:
    void fileIntegrityCheckFailed(const QnStorageResourcePtr& storage);

private:
    bool checkMetaDataIntegrity(const QnAviArchiveMetadata& metadata);
    bool checkFileName(const QnAviArchiveMetadata& metadata, const QString& fileName);
    void emitSignal(const QString& fileName);
};

} // namespace mediaserver_core
} // namespace nx
