// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>

namespace nx::vms::client::desktop {

class ServerFileCache:
    public QObject,
    public nx::vms::client::core::CommonModuleAware,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT
public:
    explicit ServerFileCache(const QString &folderName, QObject *parent = 0);
    virtual ~ServerFileCache();

    /** Get full path to cached file with fixed filename */
    virtual QString getFullPath(const QString &filename) const;

    virtual void getFileList();

    /**
     * @brief downloadFile  Downloads the file to the cache directory.
     *                      Emits fileDownloaded() when completed.
     * @param filename      Name of the file (without path).
     */
    virtual void downloadFile(const QString &filename);

    /**
     * @brief uploadFile    Uploads file already located in cache directory to the server.
     *                      Emits fileUploaded() when completed.
     * @param filename      Filename in the cache directory.
     */
    virtual void uploadFile(const QString &filename);

    virtual void deleteFile(const QString &filename);

    /** Clear cache state. */
    virtual void clear();

    static void clearLocalCache();

    enum class OperationResult {
        ok,
        disconnected,
        serverError,
        fileSystemError,
        invalidOperation,
    };

protected:
    void ensureCacheFolder();
    QString folderName() const;

    bool isConnectedToServer() const;
signals:
    void fileDownloaded(const QString& filename, OperationResult status);
    void delayedFileDownloaded(const QString& filename, OperationResult status);

    void fileUploaded(const QString& filename, OperationResult status);
    void delayedFileUploaded(const QString& filename, OperationResult status);

    void fileDeleted(const QString& filename, OperationResult status);
    void delayedFileDeleted(const QString& filename, OperationResult status);

    void fileListReceived(const QStringList& filenames, OperationResult status);
    void delayedFileListReceived(const QStringList& filenames, OperationResult status);

private:
    const QString m_folderName;
    QHash<int, QString> m_loading;
    QHash<int, QString> m_uploading;
    QHash<int, QString> m_deleting;
};

} // namespace nx::vms::client::desktop
