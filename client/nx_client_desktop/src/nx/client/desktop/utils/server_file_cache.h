#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <client_core/connection_context_aware.h>

#include <nx_ec/ec_api.h>

namespace nx {
namespace client {
namespace desktop {

class ServerFileCache : public QObject, public QnConnectionContextAware
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

    static qint64 maximumFileSize();

    enum class OperationResult {
        ok,
        disconnected,
        serverError,
        fileSystemError,
        sizeLimitExceeded,
        invalidOperation

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
private slots:
    void at_fileLoaded( int handle, ec2::ErrorCode errorCode, const QByteArray& data );
    void at_fileUploaded(int handle, ec2::ErrorCode errorCode);
    void at_fileDeleted(int handle, ec2::ErrorCode errorCode);
private:
    const QString m_folderName;
    QHash<int, QString> m_loading;
    QHash<int, QString> m_uploading;
    QHash<int, QString> m_deleting;
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::ServerFileCache::OperationResult)
