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

    QString getPath(int id) const;
signals:
    void imageLoaded(int id);
    void imageStored(int id);
private slots:
    void at_fileLoaded(int handle, const QByteArray &data);

    void saveImageDebug(const QImage& image);
private:
    QHash<int, int> m_loading;
};

#endif // APP_SERVER_FILE_CACHE_H
