#ifndef QN_CLIENT_MODEL_TYPES_H
#define QN_CLIENT_MODEL_TYPES_H

#include <utils/common/uuid.h>
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
#include <utils/common/model_functions_fwd.h>
#include <utils/common/software_version.h>
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
class QnWorkbenchState {
public:
    QnWorkbenchState(): currentLayoutIndex(-1) {}

    int currentLayoutIndex;
    QList<QnUuid> layoutUuids;
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


/**
 * Mapping from resource physical id to aspect ratio.
 */
typedef QHash<QString, qreal> QnAspectRatioHash;
Q_DECLARE_METATYPE(QnAspectRatioHash)


typedef QList<QnSoftwareVersion> QnSoftwareVersionList;
Q_DECLARE_METATYPE(QnSoftwareVersionList)

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


// -------------------------------------------------------------------------- //
// QnClientBackground
// -------------------------------------------------------------------------- //
/**
 * Set of options how to display client background.
 */
struct QnClientBackground {
    QnClientBackground():
        animationEnabled(true),
        animationMode(Qn::DefaultAnimation),
        animationCustomColor(QColor()),
        animationPeriodSec(120),
        imageEnabled(false),
        imageMode(Qn::StretchImage),
        imageOpacity(0.5)
    {}

    bool animationEnabled;
    Qn::BackgroundAnimationMode animationMode;
    QColor animationCustomColor;
    int animationPeriodSec;

    bool imageEnabled;
    QString imageName;
    QString imageOriginalName;
    Qn::ImageBehaviour imageMode;
    qreal imageOpacity;

    qreal actualImageOpacity() const {
        return imageEnabled ? imageOpacity : 0.0;
    }
};
#define QnClientBackground_Fields (animationEnabled)(animationMode)(animationCustomColor)(animationPeriodSec)(imageEnabled)(imageName)(imageOriginalName)(imageMode)(imageOpacity)

QN_FUSION_DECLARE_FUNCTIONS(QnClientBackground, (datastream)(metatype));

#endif // QN_CLIENT_MODEL_TYPES_H
