#include "app_server_notification_cache.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>

QnAppServerNotificationCache::QnAppServerNotificationCache(QObject *parent) :
    base_type(parent)
{
}

QnAppServerNotificationCache::~QnAppServerNotificationCache() {

}

void QnAppServerNotificationCache::storeSound(const QString &filePath) {
    //TODO: #GDM Threaded sound converted, target path in the cache folder

    QString newName = getFullPath(QFileInfo(filePath).fileName());
    QFile::copy(filePath, newName);

    //TODO :#GDM call this in at_soundConverted
    uploadFile(QFileInfo(newName).fileName());
}
