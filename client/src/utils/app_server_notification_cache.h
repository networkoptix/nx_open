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
    
    static QString titleTag();

    void storeSound(const QString &filePath, int maxLengthMSecs = -1);
private slots:
    void at_soundConverted(const QString &filePath);
    
    void at_fileListReceived(const QStringList &filenames, bool ok);
    void at_fileDownloaded(const QString &filename, bool ok);

};

#endif // APP_SERVER_NOTIFICATION_CACHE_H
