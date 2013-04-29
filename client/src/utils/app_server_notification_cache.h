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
    
signals:
    
public slots:
    
};

#endif // APP_SERVER_NOTIFICATION_CACHE_H
