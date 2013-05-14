#ifndef APP_SERVER_NOTIFICATION_CACHE_H
#define APP_SERVER_NOTIFICATION_CACHE_H

#include <QtGui/QStandardItemModel>

#include <utils/app_server_file_cache.h>

class QnNotificationSoundModel;

class QnAppServerNotificationCache : public QnAppServerFileCache
{
    Q_OBJECT

    typedef QnAppServerFileCache base_type;
public:
    explicit QnAppServerNotificationCache(QObject *parent = 0);
    ~QnAppServerNotificationCache();
    
    void storeSound(const QString &filePath, int maxLengthMSecs = -1, const QString &customTitle = QString());

    QnNotificationSoundModel* persistentGuiModel() const;
private slots:
    void at_soundConverted(const QString &filePath);

    void at_fileListReceived(const QStringList &filenames, bool ok);
    void at_fileAdded(const QString &filename, bool ok);
private:
    QnNotificationSoundModel* m_model;
};

#endif // APP_SERVER_NOTIFICATION_CACHE_H
