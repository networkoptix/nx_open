#ifndef QN_CAMERA_BOOKMARK_H
#define QN_CAMERA_BOOKMARK_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QStringList>
#include <QtCore/QUuid>

#include "camera_bookmark_fwd.h"

#include <utils/common/model_functions_fwd.h>

struct QnCameraBookmark {
    QUuid guid;
    QString name;
    QString description;
    qint64 timeout;         /**< Time during which recorded period should be preserved, ms. */

        /** Start time in milliseconds. */
    qint64 startTimeMs;

    /** Duration in milliseconds. 
     * 
     * -1 if duration is infinite or unknown. It may be the case if this time period 
     * represents a video chunk that is being recorded at the moment. */
    qint64 durationMs;

    qint64 endTimeMs() const;

    QStringList tags;

    QnCameraBookmark():
        startTimeMs(0),
        durationMs(0),
        timeout(-1)
    {}
};

struct QnCameraBookmarkSearchFilter {
    //QString nameFilter;
    //QString descriptionFilter;
    
    qint64 minStartTimeMs;
    qint64 maxStartTimeMs;

    //qint64 minEndTimeMs;
    //qint64 maxEndTimeMs;

    qint64 minDurationMs;

    QStringList tags;

    QnCameraBookmarkSearchFilter();

};

QDebug operator<<(QDebug dbg, const QnCameraBookmark &bookmark);

Q_DECLARE_METATYPE(QnCameraBookmark)
Q_DECLARE_TYPEINFO(QnCameraBookmark, Q_MOVABLE_TYPE);

Q_DECLARE_METATYPE(QnCameraBookmarkList)
Q_DECLARE_METATYPE(QnCameraBookmarkTagsUsage)

#endif //QN_CAMERA_BOOKMARK_H
