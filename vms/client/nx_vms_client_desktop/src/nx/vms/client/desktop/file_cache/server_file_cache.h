// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <utility>

#include <QtCore/QHash>

#include <api/server_rest_connection_fwd.h>
#include <nx/vms/client/desktop/system_context_aware.h>
#include <utils/common/delayed.h>

#include "file_cache.h"

namespace nx::vms::client::desktop {

class ServerFileCache:
    public FileCache,
    public SystemContextAware
{
    Q_OBJECT

public:
    explicit ServerFileCache(
        SystemContext* systemContext,
        const QString& folderName,
        QObject* parent = nullptr);
    virtual ~ServerFileCache() override;

    void getFileList();

    virtual QString absoluteFilePath(const QString& unsafeFilename) const override;

    void downloadFile(const QString& unsafeFilename);
    void uploadFile(const QString& unsafeFilename);
    void deleteFile(const QString& unsafeFilename);

    virtual void clear() override;

signals:
    void fileDownloaded(
        const QString& filename, OperationResult status, const QString& absolutePath);
    void fileUploaded(const QString& filename, OperationResult status);
    void fileDeleted(const QString& filename, OperationResult status);
    void fileListReceived(const QStringList& filenames, OperationResult status);

protected:
    virtual QString cacheFolder() const override;
    QString folderName() const;
    QString relativeFilePath(const QString& filename) const;
    bool isConnectedToServer() const;

private:
    // Queue an emission of `signal` via the event loop, so callers receive results
    // asynchronously even when an error is detected synchronously inside the request method.
    template <typename Signal, typename... Args>
    void emitQueued(Signal signal, Args... args)
    {
        executeLater(
            [this, signal, ...args = std::move(args)]() mutable
            {
                (this->*signal)(std::move(args)...);
            },
            this);
    }

private:
    const QString m_folderName;
    QHash<QString, rest::Handle> m_loading;
    QHash<QString, rest::Handle> m_uploading;
    QHash<QString, rest::Handle> m_deleting;
};

} // namespace nx::vms::client::desktop
