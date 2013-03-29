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

    QImage getImage(int id);
signals:
    void imageLoaded(int id, const QImage& image);
private slots:
    void at_fileLoaded(int handle, const QImage& image);
private:
    QString getPath(int id) const;

    QHash<int, int> m_loading;
};

#endif // APP_SERVER_FILE_CACHE_H
