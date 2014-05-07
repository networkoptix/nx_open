#ifndef SERVER_UPDATE_UTIL_H
#define SERVER_UPDATE_UTIL_H

#include <QtCore/QObject>
#include <QtCore/QHash>
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

    bool processUpdate(const QString &updateId, QIODevice *ioDevice);

private:
    static QnServerUpdateTool *m_instance;

    struct Chunk {
        qint64 start;
        qint64 end;

        Chunk(qint64 start, qint64 end) : start(start), end(end) {}
    };

    struct TransferInfo {
        QScopedPointer<QFile> file;
        qint64 length;
        QList<Chunk> chunks;

        TransferInfo();
        ~TransferInfo();

        void addChunk(qint64 start, qint64 end);
        bool isComplete() const;
    };

    QHash<QString, TransferInfo*> m_transfers;
    QSet<QString> m_bannedUpdates;
};

#endif // SERVER_UPDATE_UTIL_H
