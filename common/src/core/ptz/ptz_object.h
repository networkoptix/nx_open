#ifndef QN_PTZ_OBJECT_H
#define QN_PTZ_OBJECT_H

#include <common/common_globals.h>

#include <QtCore/QString>

struct QnPtzObject {
    Qn::PtzObjectType type;
    QString id;
};

Q_DECLARE_METATYPE(QnPtzObject)

#endif // QN_PTZ_OBJECT_H
