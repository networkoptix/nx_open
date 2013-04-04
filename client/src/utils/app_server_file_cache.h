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

    void loadImage(int id);
    void storeImage(const QString &filename);

    QString getFolder() const;
    QString getPath(int id) const;
    QString getUploadingPath() const;
signals:
    void imageLoaded(int id);
    void imageStored(int id);
private slots:
    void at_imageConverted(int tag);
    void at_fileLoaded(int handle, const QByteArray &data);
    void at_fileUploaded(int handle, int id);
private:
    QHash<int, int> m_loading;
    int m_uploadingHandle;
};

#endif // APP_SERVER_FILE_CACHE_H
