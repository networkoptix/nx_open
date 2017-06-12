#pragma once

#include <QtGui/QStandardItemModel>

#include <nx/client/desktop/utils/server_file_cache.h>

class QnNotificationSoundModel;

namespace nx {
namespace client {
namespace desktop {

class ServerNotificationCache : public ServerFileCache
{
    Q_OBJECT

    typedef ServerFileCache base_type;
public:
    explicit ServerNotificationCache(QObject *parent = 0);
    ~ServerNotificationCache();

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

    void at_fileListReceived(const QStringList &filenames, OperationResult status);
    void at_fileAdded(const QString &filename, OperationResult status);
    void at_fileRemoved(const QString &filename, OperationResult status);
private:
    QnNotificationSoundModel* m_model;
    QHash<QString, int> m_updatingFiles;
};

} // namespace desktop
} // namespace client
} // namespace nx
