#ifndef QN_CAMERA_BOOKMARK_H
#define QN_CAMERA_BOOKMARK_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QStringList>
#include <QtCore/QUuid>

struct QnCameraBookmark {
    QUuid guid;
    qint64 startTime;
    qint64 endTime;
    QString name;
    QString description;
    int colorIndex;         /**< Index of color to be used on the timeline. */
    qint64 lockTime;        /**< Time during which recorded period should be preserved, ms. */
    QStringList tags;

    QnCameraBookmark():
        startTime(0),
        endTime(-1),
        colorIndex(0),
        lockTime(0)
    {}

    friend bool operator==(const QnCameraBookmark &l, const QnCameraBookmark &r) {
        return (l.guid == r.guid
            && l.startTime == r.startTime
            && l.endTime == r.endTime
            && l.name == r.name
            && l.description == r.description
            && l.colorIndex == r.colorIndex
            && l.lockTime == r.lockTime
            && l.tags == r.tags
            );
    }
};

Q_DECLARE_METATYPE(QnCameraBookmark);
Q_DECLARE_TYPEINFO(QnCameraBookmark, Q_MOVABLE_TYPE);

typedef QList<QnCameraBookmark> QnCameraBookmarkList;
typedef QHash<QUuid, QnCameraBookmark> QnCameraBookmarkMap;

Q_DECLARE_METATYPE(QnCameraBookmarkList);
Q_DECLARE_METATYPE(QnCameraBookmarkMap);

#endif //QN_CAMERA_BOOKMARK_H