#ifndef QN_CAMERA_BOOKMARK_H
#define QN_CAMERA_BOOKMARK_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QStringList>
#include <QtCore/QUuid>

#include <recording/time_period.h>

#include <utils/common/model_functions_fwd.h>

class QnCameraBookmark: public QnTimePeriod {
public:
    QUuid guid;
    QString name;
    QString description;
    qint64 timeout;         /**< Time during which recorded period should be preserved, ms. */
    QStringList tags;

    QnCameraBookmark():
        QnTimePeriod(),
        timeout(-1)
    {}

    friend bool operator==(const QnCameraBookmark &l, const QnCameraBookmark &r) {
        return (l.guid == r.guid
            && l.startTimeMs == r.startTimeMs 
            && l.durationMs == r.durationMs
            && l.name == r.name
            && l.description == r.description
            && l.timeout == r.timeout
            && l.tags == r.tags
            );
    }
};

typedef QStringList QnCameraBookmarkTagsUsage;

QDebug operator<<(QDebug dbg, const QnCameraBookmark &bookmark);

Q_DECLARE_TYPEINFO(QnCameraBookmark, Q_MOVABLE_TYPE);

typedef QList<QnCameraBookmark> QnCameraBookmarkList;
typedef QHash<QUuid, QnCameraBookmark> QnCameraBookmarkMap;

Q_DECLARE_METATYPE(QnCameraBookmarkList);
Q_DECLARE_METATYPE(QnCameraBookmarkMap);
Q_DECLARE_METATYPE(QnCameraBookmarkTagsUsage);

QN_FUSION_DECLARE_FUNCTIONS(QnCameraBookmark, (metatype)(sql)(json))

#endif //QN_CAMERA_BOOKMARK_H
