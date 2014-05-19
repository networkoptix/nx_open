#ifndef SERVER_UPDATE_UTIL_H
#define SERVER_UPDATE_UTIL_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QSet>

#include <utils/common/system_information.h>
#include <utils/common/software_version.h>

class QFile;
class QIODevice;

class QnServerUpdateTool : public QObject {
    Q_OBJECT
public:
    struct UpdateInformation {
        QnSystemInformation systemInformation;
        QnSoftwareVersion version;
    };

    static QnServerUpdateTool *instance();

    bool addUpdateFile(const QString &updateId, const QByteArray &data);
    bool addUpdateFileChunk(const QString &updateId, const QByteArray &data, qint64 offset);
    bool installUpdate(const QString &updateId);
    bool uploadAndInstallUpdate(const QString &updateId, const QByteArray &data, const QString &targetId);

    ~QnServerUpdateTool();

protected:
    explicit QnServerUpdateTool();

private:
    bool processUpdate(const QString &updateId, QIODevice *ioDevice);
    void sendReply(int code = false);
    void addChunk(qint64 offset, int m_length);
    bool isComplete() const;
    void clearUpdatesLocation();

private:
    static QnServerUpdateTool *m_instance;

    QString m_updateId;
    QScopedPointer<QFile> m_file;

    qint64 m_length;
    QMap<qint64, int> m_chunks;
    qint64 m_replyTime;

    QSet<QString> m_bannedUpdates;
};

#endif // SERVER_UPDATE_UTIL_H
