#ifndef VIDEO_SERVER_CAMERAS_DATA_H
#define VIDEO_SERVER_CAMERAS_DATA_H

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QList>

#include <utils/common/json.h>

/**
 * State of the running manual camera search process.
 */
struct QnManualCameraSearchStatus {
    enum State {
        Init,
        CheckingOnline,
        CheckingHost,
        Finished,
        Aborted,

        Count
    };

    QnManualCameraSearchStatus(): state(Aborted) {}
    QnManualCameraSearchStatus(State state, int current, int total):
        state(state), current(current), total(total){}

    /** Current state of the process. */
    int state;

    /** Index of currently processed element. */
    int current;

    /** Number of elements on the current stage. */
    int total;
};

QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnManualCameraSearchStatus, (state)(current)(total), inline)

struct QnManualCameraSearchSingleCamera {
    QString name;
    QString url;
    QString manufacturer;
    bool existsInPool;

    QnManualCameraSearchSingleCamera(){}

    QnManualCameraSearchSingleCamera(const QString &name, const QString &url, const QString &manufacturer, bool existsInPool):
        name(name), url(url), manufacturer(manufacturer), existsInPool(existsInPool) {}

    QString toString() const {
        return QString(QLatin1String("%1 (%2 - %3)")).arg(name).arg(url).arg(manufacturer);
    }
};

typedef QList<QnManualCameraSearchSingleCamera> QnManualCameraSearchCameraList;

QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnManualCameraSearchSingleCamera, (name)(url)(manufacturer)(existsInPool), inline)

/**
 * Status of the manual camera search process: state and results by the time.
 */
struct QnManualCameraSearchProcessStatus {
    QnManualCameraSearchStatus status;
    QnManualCameraSearchCameraList cameras;
};

/**
 * Manual camera search process request reply: process uuid, state and results by the time.
 */
struct QnManualCameraSearchProcessReply {

    QnManualCameraSearchProcessReply() {}

    QnManualCameraSearchProcessReply(const QUuid &uuid, const QnManualCameraSearchProcessStatus &processStatus):
        processUuid(uuid), status(processStatus.status), cameras(processStatus.cameras) {}

    QUuid processUuid;
    QnManualCameraSearchStatus status;
    QnManualCameraSearchCameraList cameras;
};

QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnManualCameraSearchProcessReply, (status)(processUuid)(cameras), inline)

Q_DECLARE_METATYPE(QnManualCameraSearchProcessReply)

#endif // VIDEO_SERVER_CAMERAS_DATA_H
