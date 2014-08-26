#include "mserver_conflict_business_event.h"

#include "core/resource/resource.h"

QStringList QnCameraConflictList::encode() const {
    QStringList result;
    foreach(const QString &server, camerasByServer.keys()) {
        result.append(server);
        QStringList cameras = camerasByServer[server];
        result.append(QString::number(cameras.size()));
        result.append(cameras);
    }
    return result;
}

void QnCameraConflictList::decode(const QStringList &encoded) {
    enum encodeState {
        Server,
        Size,
        Cameras
    };

    encodeState state = Server;
    int counter = 0;
    QString curServer;

    foreach (const QString &value, encoded) {
        switch(state) {
        case Server:
            curServer = value;
            state = Size;
            break;
        case Size:
            counter = value.toInt();
            state = Cameras;
            break;
        case Cameras:
            --counter;
            camerasByServer[curServer].append(value);
            if (counter <= 0)
                state = Server;
            break;
        default:
            break;
        }
    }
}

QnMServerConflictBusinessEvent::QnMServerConflictBusinessEvent(
        const QnResourcePtr& mServerRes,
        qint64 timeStamp,
        const QnCameraConflictList &conflictList):
    base_type(QnBusiness::ServerConflictEvent,
                            mServerRes,
                            timeStamp)
{
    m_cameraConflicts = conflictList;

    m_source = m_cameraConflicts.sourceServer;
    m_conflicts = m_cameraConflicts.encode();
}
