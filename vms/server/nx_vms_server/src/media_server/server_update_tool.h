#ifndef SERVER_UPDATE_UTIL_H
#define SERVER_UPDATE_UTIL_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QSet>

#include <common/common_module_aware.h>

#include <nx/utils/thread/mutex.h>
#include <nx/vms/server/server_module_aware.h>

class QFile;
class QIODevice;
class QnZipExtractor;

class QnServerUpdateTool:
    public QObject,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT

public:
    enum ReplyCode
    {
        NoReply = -1,
        UploadFinished = -2,
        NoError = -3,
        UnknownError = -4,
        NoFreeSpace = -5
    };

    enum class UpdateType
    {
        Instant,
        Delayed
    };

    QnServerUpdateTool(QnMediaServerModule* serverModule);
    virtual ~QnServerUpdateTool() override;

    bool addUpdateFile(const QString& updateId, const QByteArray& data);
    qint64 addUpdateFileChunkSync(const QString& updateId, const QByteArray& data, qint64 offset);
    void addUpdateFileChunkAsync(const QString& updateId, const QByteArray& data, qint64 offset);
    bool installUpdate(const QString& updateId, UpdateType updateType = UpdateType::Instant);

    void removeUpdateFiles(const QString& updateId);

private:
    qint64 addUpdateFileChunkInternal(const QString& updateId, const QByteArray& data, qint64 offset);
    ReplyCode processUpdate(const QString& updateId, QIODevice* ioDevice, bool sync);
    void sendReply(int code);
    void clearUpdatesLocation(const QString& idToLeave = QString());
    bool initializeUpdateLog(const QString& targetVersion, QString* logFileName) const;
    QDir getUpdatesDir() const;
    QDir getUpdateDir(const QString& updateId) const;
    QString getUpdateFilePath(const QString& updateId) const;
private slots:
    void at_zipExtractor_extractionFinished(int error);

private:
    QnMutex m_mutex;

    QString m_updateId;
    QScopedPointer<QFile> m_file;

    QByteArray m_fileMd5;
    qint64 m_replyTime;

    QSet<QString> m_bannedUpdates;

    QScopedPointer<QnZipExtractor> m_zipExtractor;
};

#endif // SERVER_UPDATE_UTIL_H
