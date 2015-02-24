#ifndef APP_SERVER_FILE_CACHE_H
#define APP_SERVER_FILE_CACHE_H

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <nx_ec/ec_api.h>

#include <ui/workbench/workbench_context_aware.h>

class QnAppServerFileCache : public QObject
{
    Q_OBJECT
public:
    explicit QnAppServerFileCache(const QString &folderName, QObject *parent = 0);
    virtual ~QnAppServerFileCache();

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

protected:
    void ensureCacheFolder();
    QString folderName() const;

    bool isConnectedToServer() const;
signals:
    void fileDownloaded(const QString& filename, bool ok);
    void delayedFileDownloaded(const QString& filename, bool ok);

    void fileUploaded(const QString& filename, bool ok);
    void delayedFileUploaded(const QString& filename, bool ok);

    void fileDeleted(const QString& filename, bool ok);
    void delayedFileDeleted(const QString& filename, bool ok);

    void fileListReceived(const QStringList& filenames, bool ok);
    void delayedFileListReceived(const QStringList& filenames, bool ok);
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

#endif // APP_SERVER_FILE_CACHE_H
