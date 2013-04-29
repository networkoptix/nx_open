#ifndef APP_SERVER_FILE_CACHE_H
#define APP_SERVER_FILE_CACHE_H

#include <QtCore/QObject>
#include <QtCore/QHash>

class QnAppServerFileCache : public QObject
{
    Q_OBJECT
public:
    explicit QnAppServerFileCache(QObject *parent = 0);
    ~QnAppServerFileCache();

    /** Get full path to cached file with fixed filename */
    QString getFullPath(const QString &filename) const;

    void uploadFile(const QString &filename);
    void downloadFile(const QString &filename);
signals:
    void fileDownloaded(const QString& filename, bool ok);
    void fileUploaded(const QString& filename, bool ok);
private slots:

    void at_fileLoaded(int status, const QByteArray& data, int handle);
    void at_fileUploaded(int status, int handle);
private:
    QHash<int, QString> m_loading;
    QHash<int, QString> m_uploading;
};

#endif // APP_SERVER_FILE_CACHE_H
