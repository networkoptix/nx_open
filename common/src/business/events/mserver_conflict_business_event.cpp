#include "mserver_conflict_business_event.h"

#include "core/resource/resource.h"
#include "utils/network/module_information.h"

QStringList QnCameraConflictList::encode() const {
    QStringList result;
    for(const QString &server: camerasByServer.keys()) {
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

    for (const QString &value: encoded) {
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

QnMServerConflictBusinessEvent::QnMServerConflictBusinessEvent(
        const QnResourcePtr &mServerRes,
        qint64 timeStamp,
        const QnModuleInformation &conflictModule,
        const QUrl &conflictUrl):
    base_type(QnBusiness::ServerConflictEvent,
              mServerRes,
              timeStamp)
{
    Q_UNUSED(conflictModule)
    m_source = lit("%1:%2").arg(conflictUrl.host()).arg(conflictUrl.port());
}
