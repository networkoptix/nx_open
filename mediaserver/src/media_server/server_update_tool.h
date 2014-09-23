#ifndef SERVER_UPDATE_UTIL_H
#define SERVER_UPDATE_UTIL_H

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QMap>
#include <QtCore/QSet>

#include <utils/common/system_information.h>
#include <utils/common/software_version.h>
#include <utils/common/singleton.h>

class QFile;
class QIODevice;
class QNetworkAccessManager;

class QnServerUpdateTool : public QObject, public Singleton<QnServerUpdateTool> {
    Q_OBJECT
public:
    struct UpdateInformation {
        QnSystemInformation systemInformation;
        QnSoftwareVersion version;
    };

    QnServerUpdateTool();
    ~QnServerUpdateTool();

    bool addUpdateFile(const QString &updateId, const QByteArray &data);
    void addUpdateFileChunk(const QString &updateId, const QByteArray &data, qint64 offset);
    bool installUpdate(const QString &updateId);

private:
    bool processUpdate(const QString &updateId, QIODevice *ioDevice);
    void sendReply(int code = false);
    void addChunk(qint64 offset, int m_length);
    bool isComplete() const;
    void clearUpdatesLocation();

private slots:
    void at_uploadFinished();

private:
    QString m_updateId;
    QScopedPointer<QFile> m_file;

    qint64 m_length;
    QMap<qint64, int> m_chunks;
    qint64 m_replyTime;

    QSet<QString> m_bannedUpdates;
};

#endif // SERVER_UPDATE_UTIL_H
