#ifndef QN_PTZ_OBJECT_H
#define QN_PTZ_OBJECT_H

#include <boost/operators.hpp>

#include <QtCore/QString>

#include <common/common_globals.h>

struct QnPtzObject: public boost::equality_comparable1<QnPtzObject> {
    QnPtzObject(): type(Qn::InvalidPtzObject) {}
    QnPtzObject(Qn::PtzObjectType type, const QString &id): type(type), id(id) {}

    friend bool operator==(const QnPtzObject &l, const QnPtzObject &r);

    Qn::PtzObjectType type;
    QString id;
};
#define QnPtzObject_Fields (type)(id)

Q_DECLARE_METATYPE(QnPtzObject)

#endif // QN_PTZ_OBJECT_H
