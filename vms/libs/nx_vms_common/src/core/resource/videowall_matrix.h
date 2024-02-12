// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef VIDEO_WALL_MATRIX_H
#define VIDEO_WALL_MATRIX_H

#include <QtCore/QHash>
#include <QtCore/QList>

#include <nx/utils/uuid.h>

class QnVideoWallMatrix
{
public:
    QnVideoWallMatrix() {}

    nx::Uuid uuid;
    QString name;
    QHash<nx::Uuid, nx::Uuid> layoutByItem;

    friend bool operator==(const QnVideoWallMatrix &l, const QnVideoWallMatrix &r) {
        return (l.uuid == r.uuid &&
                l.name == r.name &&
                l.layoutByItem == r.layoutByItem);
    }
};


typedef QList<QnVideoWallMatrix> QnVideoWallMatrixList;
typedef QHash<nx::Uuid, QnVideoWallMatrix> QnVideoWallMatrixMap;


#endif // VIDEO_WALL_MATRIX_H
