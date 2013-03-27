#ifndef QN_CLIENT_MODEL_TYPES_H
#define QN_CLIENT_MODEL_TYPES_H

#include <QtCore/QUuid>
#include <QtCore/QList>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtCore/QDataStream>

#include <utils/common/struct_functions.h>

// -------------------------------------------------------------------------- //
// QnWorkbenchState
// -------------------------------------------------------------------------- //
// TODO: #Elric doxydocs
class QnWorkbenchState {
public:
    QnWorkbenchState(): currentLayoutIndex(-1) {}

    int currentLayoutIndex;
    QList<QString> layoutUuids;
};

QN_DEFINE_STRUCT_FUNCTIONS(QnWorkbenchState, (qdatastream), (currentLayoutIndex)(layoutUuids), inline);

/**
 * Mapping from user name to workbench state.
 */
typedef QHash<QString, QnWorkbenchState> QnWorkbenchStateHash;

Q_DECLARE_METATYPE(QnWorkbenchState);
Q_DECLARE_METATYPE(QnWorkbenchStateHash);


// -------------------------------------------------------------------------- //
// QnServerStorageState
// -------------------------------------------------------------------------- //
struct QnServerStorageKey {
    QnServerStorageKey() {}
    QnServerStorageKey(const QUuid &serverUuid, const QString &storagePath): serverUuid(serverUuid), storagePath(storagePath) {}

    QUuid serverUuid;
    QString storagePath;
};

QN_DEFINE_STRUCT_FUNCTIONS(QnServerStorageKey, (qdatastream)(eq)(qhash), (serverUuid)(storagePath), inline);

typedef QHash<QnServerStorageKey, qint64> QnServerStorageStateHash;

Q_DECLARE_METATYPE(QnServerStorageKey);
Q_DECLARE_METATYPE(QnServerStorageStateHash);


// -------------------------------------------------------------------------- //
// QnLicenseWarningState
// -------------------------------------------------------------------------- //
struct QnLicenseWarningState {
    QnLicenseWarningState(): ignore(false) {}
    QnLicenseWarningState(const QDateTime &lastWarningTime, bool ignore): lastWarningTime(lastWarningTime), ignore(ignore) {}

    QDateTime lastWarningTime;
    bool ignore;
};

QN_DEFINE_STRUCT_FUNCTIONS(QnLicenseWarningState, (qdatastream), (lastWarningTime)(ignore), inline);

/**
 * Mapping from license key to license warning state.
 */
typedef QHash<QByteArray, QnLicenseWarningState> QnLicenseWarningStateHash;

Q_DECLARE_METATYPE(QnLicenseWarningState);
Q_DECLARE_METATYPE(QnLicenseWarningStateHash);


#endif // QN_CLIENT_MODEL_TYPES_H
