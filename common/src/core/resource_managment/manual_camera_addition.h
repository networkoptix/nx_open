#ifndef MANUAL_CAMERA_ADDITION_H
#define MANUAL_CAMERA_ADDITION_H

#include <utils/common/json.h>

struct QnManualCameraSearchStatus {
    enum Status {
        Init,
        CheckingOnline,
        CheckingHost,
        Finished,
        Aborted,

        Count
    };

    QnManualCameraSearchStatus(){}
    QnManualCameraSearchStatus(Status status, int current, int total):
        status(status), current(current), total(total){}

    /** Current status of the process. */
    int status;

    /** Index of currently processed element. */
    int current;

    /** Number of elements on the current stage. */
    int total;

    /**
     * Datetime in msecs since epoch when the data was requested last time.
     * Required to abort the process if the client was closed unexpectedly.
     */
    quint64 lastRequestTimestamp;
};

QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnManualCameraSearchStatus, (status)(current)(total), inline)

struct QnManualCameraSearchSingleCamera {
    QString name;
    QString url;
    QString manufacturer;
};

QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnManualCameraSearchSingleCamera, (name)(url)(manufacturer), inline)

/**
 *  Reply
 */
struct QnManualCameraSearchProcessReply {
    QnManualCameraSearchStatus status;
    QUuid processUuid;
    QList<QnManualCameraSearchSingleCamera> cameras;
};

QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnManualCameraSearchProcessReply, (status)(processUuid)(cameras), inline)

#endif // MANUAL_CAMERA_ADDITION_H
