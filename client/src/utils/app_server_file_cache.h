#ifndef APP_SERVER_FILE_CACHE_H
#define APP_SERVER_FILE_CACHE_H

#include <QObject>
#include <QtGui/QImage>

class QnAppServerFileCache : public QObject
{
    Q_OBJECT
public:
    explicit QnAppServerFileCache(QObject *parent = 0);
    ~QnAppServerFileCache();

    void loadImage(const QString &filename);
    void storeImage(const QString &filePath);

    /** Get full path to cached file with fixed filename */
    QString getFullPath(const QString &filename) const;
signals:
    void imageLoaded(const QString& filename, bool ok);
    void imageStored(const QString& filename, bool ok);
private slots:
    void at_imageConverted(const QString &filePath);
    void at_fileLoaded(int status, const QByteArray& data, int handle);
    void at_fileUploaded(int status, int handle);
private:
    QHash<int, QString> m_loading;
    QHash<int, QString> m_uploading;
};

#endif // APP_SERVER_FILE_CACHE_H
