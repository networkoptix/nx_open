#pragma once

#include <nx/utils/uuid.h>
#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QObject>
#include <QtCore/QWeakPointer>
#include <QtCore/QMetaType>
#include <QtCore/QDataStream>
#include <QtGui/QVector3D>

#include <client/client_globals.h>

#include <utils/common/id.h>
#include <nx/fusion/model_functions_fwd.h>
#include <recording/time_period.h>


// -------------------------------------------------------------------------- //
// Qt-based
// -------------------------------------------------------------------------- //
typedef QHash<QString, QWeakPointer<QObject> > QnWeakObjectHash;

Q_DECLARE_METATYPE(QnWeakObjectHash)

// -------------------------------------------------------------------------- //
// QnThumbnailsSearchState
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
class QnWorkbenchState
{
public:
    QnWorkbenchState();
    QnUuid userId;          /*< Id of the user. Works as first part of the key. */
    QnUuid localSystemId;   /*< Id of the system. Works as second part of the key. */
    QnUuid currentLayoutId;
    QnUuid runningTourId;
    QList<QnUuid> layoutUuids;

    bool isValid() const;
};
#define QnWorkbenchState_Fields \
    (userId)(localSystemId)(currentLayoutId)(runningTourId)(layoutUuids)

using QnWorkbenchStateList = QList<QnWorkbenchState>;

QN_FUSION_DECLARE_FUNCTIONS(QnWorkbenchState, (json)(metatype));

// -------------------------------------------------------------------------- //
// QnServerStorageState
// -------------------------------------------------------------------------- //
struct QnServerStorageKey {
    QnServerStorageKey() {}
    QnServerStorageKey(const QnUuid &serverUuid, const QString &storagePath): serverUuid(serverUuid), storagePath(storagePath) {}

    QnUuid serverUuid;
    QString storagePath;
};

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

inline bool deserialize(const QString& value, QnPtzHotkeyHash* target)
{
    Q_UNUSED(value);
    Q_UNUSED(target);
    Q_ASSERT_X(0, Q_FUNC_INFO, "Not implemented");
    return false;
}



// -------------------------------------------------------------------------- //
// QnBackgroundImage
// -------------------------------------------------------------------------- //
/**
 * Set of options how to display client background.
 */
struct QnBackgroundImage
{
    QnBackgroundImage();

    bool enabled;
    QString name;
    QString originalName;
    Qn::ImageBehaviour mode;
    qreal opacity;

    qreal actualImageOpacity() const;
};
#define QnBackgroundImage_Fields (enabled)(name)(originalName)(mode)(opacity)

QN_FUSION_DECLARE_FUNCTIONS(QnBackgroundImage, (json)(metatype)(eq));
