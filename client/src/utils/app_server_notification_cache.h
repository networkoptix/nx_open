#ifndef APP_SERVER_NOTIFICATION_CACHE_H
#define APP_SERVER_NOTIFICATION_CACHE_H

#include <utils/app_server_file_cache.h>

class QnAppServerNotificationCache : public QnAppServerFileCache
{
    Q_OBJECT

    typedef QnAppServerFileCache base_type;
public:
    explicit QnAppServerNotificationCache(QObject *parent = 0);
    ~QnAppServerNotificationCache();
    
    void storeSound(const QString &filePath);
private slots:
    void at_soundConverted(const QString &filePath);
    
};

#endif // APP_SERVER_NOTIFICATION_CACHE_H
