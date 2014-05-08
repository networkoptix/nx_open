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

protected:
    explicit QnServerUpdateTool();

private:
    bool processUpdate(const QString &updateId, QIODevice *ioDevice);
    void sendReply(const QString &updateId, int code = false);

private:
    static QnServerUpdateTool *m_instance;

    struct TransferInfo {
        QScopedPointer<QFile> file;

        qint64 length;
        QMap<qint64, int> chunks;
        qint64 replyTime;

        TransferInfo();
        ~TransferInfo();

        void addChunk(qint64 offset, int length);
        bool isComplete() const;
    };

    QHash<QString, TransferInfo*> m_transfers;
    QSet<QString> m_bannedUpdates;
};

#endif // SERVER_UPDATE_UTIL_H
