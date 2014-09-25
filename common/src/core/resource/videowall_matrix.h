#ifndef VIDEO_WALL_MATRIX_H
#define VIDEO_WALL_MATRIX_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <utils/common/uuid.h>

class QnVideoWallMatrix
{
public:
    QnVideoWallMatrix() {}

    QnUuid uuid;
    QString name;
    QHash<QnUuid, QnUuid> layoutByItem;

    friend bool operator==(const QnVideoWallMatrix &l, const QnVideoWallMatrix &r) {
        return (l.uuid == r.uuid &&
                l.name == r.name &&
                l.layoutByItem == r.layoutByItem);
    }
};

Q_DECLARE_METATYPE(QnVideoWallMatrix)
Q_DECLARE_TYPEINFO(QnVideoWallMatrix, Q_MOVABLE_TYPE);

typedef QList<QnVideoWallMatrix> QnVideoWallMatrixList;
typedef QHash<QnUuid, QnVideoWallMatrix> QnVideoWallMatrixMap;

Q_DECLARE_METATYPE(QnVideoWallMatrixList)
Q_DECLARE_METATYPE(QnVideoWallMatrixMap)

#endif // VIDEO_WALL_MATRIX_H
