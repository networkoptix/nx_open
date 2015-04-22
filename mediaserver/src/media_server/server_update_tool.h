#ifndef SERVER_UPDATE_UTIL_H
#define SERVER_UPDATE_UTIL_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QMutex>

#include <utils/common/system_information.h>
#include <utils/common/software_version.h>
#include <utils/common/singleton.h>

class QFile;
class QIODevice;
class QnZipExtractor;

class QnServerUpdateTool : public QObject, public Singleton<QnServerUpdateTool> {
    Q_OBJECT
public:
    enum ReplyCode {
        NoReply = -1,
        UploadFinished = -2,
        NoError = -3,
        UnknownError = -4,
        NoFreeSpace = -5
    };

    QnServerUpdateTool();
    ~QnServerUpdateTool();

    bool addUpdateFile(const QString &updateId, const QByteArray &data);
    qint64 addUpdateFileChunkSync(const QString &updateId, const QByteArray &data, qint64 offset);
    void addUpdateFileChunkAsync(const QString &updateId, const QByteArray &data, qint64 offset);
    bool installUpdate(const QString &updateId);

private:
    qint64 addUpdateFileChunkInternal(const QString &updateId, const QByteArray &data, qint64 offset);
    ReplyCode processUpdate(const QString &updateId, QIODevice *ioDevice, bool sync);
    void sendReply(int code);
    void clearUpdatesLocation(const QString &idToLeave = QString());

private slots:
    void at_zipExtractor_extractionFinished(int error);

private:
    QMutex m_mutex;

    QString m_updateId;
    QScopedPointer<QFile> m_file;

    QByteArray m_fileMd5;
    qint64 m_replyTime;

    QSet<QString> m_bannedUpdates;

    QScopedPointer<QnZipExtractor> m_zipExtractor;
};

#endif // SERVER_UPDATE_UTIL_H
