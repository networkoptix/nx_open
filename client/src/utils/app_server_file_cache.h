#ifndef APP_SERVER_FILE_CACHE_H
#define APP_SERVER_FILE_CACHE_H

#include <QtCore/QObject>
#include <QtCore/QHash>

class QnAppServerFileCache : public QObject
{
    Q_OBJECT
public:
    enum Category {
        LayoutBackground,
        NotificationSound,

        CategoryCount
    };

    explicit QnAppServerFileCache(QObject *parent = 0);
    ~QnAppServerFileCache();

    /** Get full path to cached file with fixed filename */
    QString getFullPath(const QString &filename) const;

    void getFileList();

    /**
     * @brief downloadFile  Downloads the file to the cache directory.
     *                      Emits fileDownloaded() when completed.
     * @param filename      Name of the file (without path).
     */
    void downloadFile(const QString &filename);
signals:
    void fileDownloaded(const QString& filename, bool ok);
    void fileUploaded(const QString& filename, bool ok);
    void fileListReceived(const QStringList& filenames, bool ok);

protected:
    /**
     * @brief uploadFile    Uploads file already located in cache directory to the server.
     *                      Emits fileUploaded() when completed.
     * @param filename      Filename in the cache directory.
     */
    void uploadFile(const QString &filename);

private slots:

    void at_fileLoaded(int status, const QByteArray& data, int handle);
    void at_fileUploaded(int status, int handle);
private:
    QHash<int, QString> m_loading;
    QHash<int, QString> m_uploading;
};

#endif // APP_SERVER_FILE_CACHE_H
