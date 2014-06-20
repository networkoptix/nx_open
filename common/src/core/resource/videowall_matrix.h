#ifndef VIDEO_WALL_MATRIX_H
#define VIDEO_WALL_MATRIX_H

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QUuid>

class QnVideoWallMatrix
{
public:
    QnVideoWallMatrix() {}

    QUuid uuid;
    QString name;
    QHash<QUuid, QUuid> layoutByItem;

    friend bool operator==(const QnVideoWallMatrix &l, const QnVideoWallMatrix &r) {
        return (l.uuid == r.uuid &&
                l.name == r.name &&
                l.layoutByItem == r.layoutByItem);
    }
};

Q_DECLARE_METATYPE(QnVideoWallMatrix)
Q_DECLARE_TYPEINFO(QnVideoWallMatrix, Q_MOVABLE_TYPE);

typedef QList<QnVideoWallMatrix> QnVideoWallMatrixList;
typedef QHash<QUuid, QnVideoWallMatrix> QnVideoWallMatrixMap;

Q_DECLARE_METATYPE(QnVideoWallMatrixList)
Q_DECLARE_METATYPE(QnVideoWallMatrixMap)

#endif // VIDEO_WALL_MATRIX_H
