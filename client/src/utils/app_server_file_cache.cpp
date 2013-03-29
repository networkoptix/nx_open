#include "app_server_file_cache.h"

#include <QDesktopServices>
#include <QFileInfo>

#include <api/app_server_connection.h>

QnAppServerFileCache::QnAppServerFileCache(QObject *parent) :
    QObject(parent)
{}

QnAppServerFileCache::~QnAppServerFileCache(){}


QImage QnAppServerFileCache::getImage(int id) {
    QString fullPath = getPath(id);
    QImage img(fullPath);
    if (img.isNull()) {
        if (m_loading.values().contains(id))
            return img; //we are already loading this id

        int handle = QnAppServerConnectionFactory::createConnection()->requestStoredFileAsync(
                    id,
                    this,
                    SLOT(at_fileLoaded(int, const QImage &))
        );
        m_loading.insert(handle, id);
    }
    return img;
}

int QnAppServerFileCache::appendDebug(QString path) {
    int i = 1;
    while (QFileInfo(getPath(i)).exists())
        i++;
    QImage(path).save(getPath(i));
    return i;
}

QString QnAppServerFileCache::getPath(int id) const {
    QString path = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    QUrl url = QnAppServerConnectionFactory::defaultUrl();

    return QString(QLatin1String("%1/cache/%2_%3/%4.png"))
            .arg(path)
            .arg(QLatin1String(url.encodedHost()))
            .arg(url.port())
            .arg(id);
}

void QnAppServerFileCache::at_fileLoaded(int handle, const QImage &image) {
    if (!m_loading.contains(handle))
        return;
    int id = m_loading[handle];
    m_loading.remove(handle);
    if (!image.save(getPath(id)))
        qWarning() << "Image cannot be saved" << getPath(id);
    emit imageLoaded(id, image);
}




