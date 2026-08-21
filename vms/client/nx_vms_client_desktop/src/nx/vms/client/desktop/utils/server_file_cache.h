// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>

#include <nx/vms/client/core/file_cache/file_cache.h>
#include <nx/vms/client/desktop/system_context_aware.h>

namespace nx::vms::client::desktop {

class ServerFileCache:
    public core::FileCache,
    public SystemContextAware
{
    Q_OBJECT
public:
    explicit ServerFileCache(
        SystemContext* systemContext,
        const QString& folderName,
        QObject* parent = nullptr);
    virtual ~ServerFileCache();

    /** Get full path to cached file with fixed filename */
    virtual QString getFullPath(const QString &filename) const;

    /**
     * Full path to the file with the name which is safe to use in the cache directory. Returns an
     * empty string for the names rejected by sanitizeFilename().
     */
    virtual QString absoluteFilePath(const QString& unsafeFilename) const override;

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
    virtual void clear() override;

    static void clearLocalCache();

protected:
    virtual QString cacheFolder() const override;
    QString folderName() const;
    QString relativeFilePath(const QString& filename) const;
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
