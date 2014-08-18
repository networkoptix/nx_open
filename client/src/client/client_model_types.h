#ifndef QN_CLIENT_MODEL_TYPES_H
#define QN_CLIENT_MODEL_TYPES_H

#include <QtCore/QUuid>
#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QObject>
#include <QtCore/QWeakPointer>
#include <QtCore/QMetaType>
#include <QtCore/QDataStream>
#include <QtGui/QVector3D>

#include <utils/common/id.h>
#include <utils/common/model_functions_fwd.h>
#include <recording/time_period.h>


// -------------------------------------------------------------------------- //
// Qt-based
// -------------------------------------------------------------------------- //
typedef QHash<QString, QWeakPointer<QObject> > QnWeakObjectHash;

Q_DECLARE_METATYPE(QnWeakObjectHash)

// -------------------------------------------------------------------------- //
// QnWorkbenchState
// -------------------------------------------------------------------------- //
/**
 * Additional data for a thumbnails search layout.
 */
struct QnThumbnailsSearchState {
    QnThumbnailsSearchState(): step(0) {}
    QnThumbnailsSearchState(const QnTimePeriod &period, qint64 step): period(period), step(step) {}

    QnTimePeriod period;
    qint64 step;
};

Q_DECLARE_METATYPE(QnThumbnailsSearchState)


// -------------------------------------------------------------------------- //
// QnWorkbenchState
// -------------------------------------------------------------------------- //
/**
 * Serialized workbench state. Does not contain the actual layout data, so
 * suitable for restoring the state once layouts are loaded.
 */
class QnWorkbenchState {
public:
    QnWorkbenchState(): currentLayoutIndex(-1) {}

    int currentLayoutIndex;
    QList<QUuid> layoutUuids;
};

/**
 * Mapping from user name to workbench state.
 */
typedef QHash<QString, QnWorkbenchState> QnWorkbenchStateHash;

Q_DECLARE_METATYPE(QnWorkbenchStateHash);
QN_FUSION_DECLARE_FUNCTIONS(QnWorkbenchState, (datastream)(metatype));


// -------------------------------------------------------------------------- //
// QnServerStorageState
// -------------------------------------------------------------------------- //
struct QnServerStorageKey {
    QnServerStorageKey() {}
    QnServerStorageKey(const QUuid &serverUuid, const QString &storagePath): serverUuid(serverUuid), storagePath(storagePath) {}

    QUuid serverUuid;
    QString storagePath;
};

typedef QHash<QnServerStorageKey, qint64> QnServerStorageStateHash;

Q_DECLARE_METATYPE(QnServerStorageStateHash);
QN_FUSION_DECLARE_FUNCTIONS(QnServerStorageKey, (datastream)(eq)(hash)(metatype));


// -------------------------------------------------------------------------- //
// QnLicenseWarningState
// -------------------------------------------------------------------------- //
struct QnLicenseWarningState {
    QnLicenseWarningState(qint64 lastWarningTime = 0): lastWarningTime(lastWarningTime) {}

    qint64 lastWarningTime;
};

/**
 * Mapping from license key to license warning state.
 */
typedef QHash<QByteArray, QnLicenseWarningState> QnLicenseWarningStateHash;

Q_DECLARE_METATYPE(QnLicenseWarningStateHash);
QN_FUSION_DECLARE_FUNCTIONS(QnLicenseWarningState, (datastream)(metatype));


/**
 * Mapping from resource physical id to aspect ratio.
 */
typedef QHash<QString, qreal> QnAspectRatioHash;
Q_DECLARE_METATYPE(QnAspectRatioHash)


// -------------------------------------------------------------------------- //
// QnPtzHotkey
// -------------------------------------------------------------------------- //
struct QnPtzHotkey {
    enum {
        NoHotkey = -1
    };

    QnPtzHotkey(): hotkey(NoHotkey) {}
    QnPtzHotkey(const QString &id, int hotkey): id(id), hotkey(hotkey) {}

    QString id;
    int hotkey;
};

typedef QHash<int, QString> QnPtzHotkeyHash;

QN_FUSION_DECLARE_FUNCTIONS(QnPtzHotkey, (json)(metatype));

#endif // QN_CLIENT_MODEL_TYPES_H
