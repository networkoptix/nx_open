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

    bool storeSound(const QString &filePath, int maxLengthMSecs = -1, const QString &customTitle = QString());
    bool updateTitle(const QString &filename, const QString &title);
    virtual void clear() override;

    QnNotificationSoundModel* persistentGuiModel() const;
public slots:
    void at_fileAddedEvent(const QString &filename);
    void at_fileUpdatedEvent(const QString &filename);
    void at_fileRemovedEvent(const QString &filename);
private slots:
    void at_soundConverted(const QString &filePath);

    void at_fileListReceived(const QStringList &filenames, bool ok);
    void at_fileAdded(const QString &filename, bool ok);
    void at_fileRemoved(const QString &filename, bool ok);
private:
    QnNotificationSoundModel* m_model;
    QHash<QString, int> m_updatingFiles;
};

#endif // APP_SERVER_NOTIFICATION_CACHE_H
