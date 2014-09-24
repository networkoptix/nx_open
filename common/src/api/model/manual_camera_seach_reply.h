#ifndef QN_MANUAL_CAMERA_SEARCH_REPLY_H
#define QN_MANUAL_CAMERA_SEARCH_REPLY_H

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QUuid>

#include <utils/common/model_functions_fwd.h>

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
    QnManualCameraSearchStatus(State state, quint64 current, quint64 total):
        state(state), current(current), total(total){}

    /** Current state of the process. */
    int state;

    /** Index of currently processed element. */
    qint64 current;

    /** Number of elements on the current stage. */
    qint64 total;
};

#define QnManualCameraSearchStatus_Fields (state)(current)(total)

struct QnManualCameraSearchSingleCamera {
    QString name;
    QString url;
    QString manufacturer;
    QString vendor;
    bool existsInPool;

    QnManualCameraSearchSingleCamera(){}

    QnManualCameraSearchSingleCamera(const QString &name, const QString &url, const QString &manufacturer, const QString &vendor, bool existsInPool):
        name(name), url(url), manufacturer(manufacturer), vendor(vendor), existsInPool(existsInPool) {}

    QString toString() const {
        return QString(QLatin1String("%1 (%2 - %3)")).arg(name).arg(url).arg(vendor);
    }
};

#define QnManualCameraSearchSingleCamera_Fields (name)(url)(manufacturer)(vendor)(existsInPool)

typedef QList<QnManualCameraSearchSingleCamera> QnManualCameraSearchCameraList;


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
struct QnManualCameraSearchReply {

    QnManualCameraSearchReply() {}

    QnManualCameraSearchReply(const QUuid &uuid, const QnManualCameraSearchProcessStatus &processStatus):
        processUuid(uuid), status(processStatus.status), cameras(processStatus.cameras) {}

    QUuid processUuid;
    QnManualCameraSearchStatus status;
    QnManualCameraSearchCameraList cameras;
};

#define QnManualCameraSearchReply_Fields (status)(processUuid)(cameras)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnManualCameraSearchStatus)(QnManualCameraSearchSingleCamera)(QnManualCameraSearchReply), (json)(metatype))

#endif // QN_MANUAL_CAMERA_SEARCH_REPLY_H
