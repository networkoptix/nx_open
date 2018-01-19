#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <QtCore/QList>
#include <nx/utils/uuid.h>

#include <nx/fusion/model_functions_fwd.h>

/**
 * State of the running manual camera search process.
 */
struct QnManualResourceSearchStatus {
    enum State {
        Init,
        CheckingOnline,
        CheckingHost,
        Finished,
        Aborted,

        Count
    };

    QnManualResourceSearchStatus(): state(Aborted) {}
    QnManualResourceSearchStatus(State state, quint64 current, quint64 total):
        state(state), current(current), total(total){}

    /** Current state of the process. */
    int state;

    /** Index of currently processed element. */
    qint64 current;

    /** Number of elements on the current stage. */
    qint64 total;
};

#define QnManualResourceSearchStatus_Fields (state)(current)(total)

// TODO: #wearable better split this struct in two. name, vendor & existsInPool unused in add requests.
struct QnManualResourceSearchEntry {
    QString name;
    QString url;
    QString manufacturer;
    QString vendor;
    QString uniqueId;
    bool existsInPool;

    QnManualResourceSearchEntry(): existsInPool(false) {}

    QnManualResourceSearchEntry(const QString &name, const QString &url, const QString &manufacturer, const QString &vendor, const QString& uniqueId, bool existsInPool):
        name(name), url(url), manufacturer(manufacturer), vendor(vendor), uniqueId(uniqueId), existsInPool(existsInPool) {}

    QString toString() const {
        return QString(QLatin1String("%1 (%2 - %3)")).arg(name).arg(url).arg(vendor);
    }

    bool isNull() const {
        return uniqueId.isEmpty();
    }
};

#define QnManualResourceSearchEntry_Fields (name)(url)(manufacturer)(vendor)(existsInPool)(uniqueId)

typedef QList<QnManualResourceSearchEntry> QnManualResourceSearchList;


/**
 * Status of the manual camera search process: state and results by the time.
 */
struct QnManualCameraSearchProcessStatus {
    QnManualResourceSearchStatus status;
    QnManualResourceSearchList cameras;
};

/**
 * Manual camera search process request reply: process uuid, state and results by the time.
 */
struct QnManualCameraSearchReply {

    QnManualCameraSearchReply() {}

    QnManualCameraSearchReply(const QnUuid &uuid, const QnManualCameraSearchProcessStatus &processStatus):
        processUuid(uuid), status(processStatus.status), cameras(processStatus.cameras) {}

    QnUuid processUuid;
    QnManualResourceSearchStatus status;
    QnManualResourceSearchList cameras;
};

#define QnManualCameraSearchReply_Fields (status)(processUuid)(cameras)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnManualResourceSearchStatus)(QnManualResourceSearchEntry)(QnManualCameraSearchReply), (json)(metatype))

