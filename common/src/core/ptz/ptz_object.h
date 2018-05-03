#pragma once

#include <QtCore/QString>

#include <common/common_globals.h>

struct QnPtzObject
{
    QnPtzObject(): type(Qn::InvalidPtzObject) {}
    QnPtzObject(Qn::PtzObjectType type, const QString &id): type(type), id(id) {}

    Qn::PtzObjectType type;
    QString id;
};
#define QnPtzObject_Fields (type)(id)

Q_DECLARE_METATYPE(QnPtzObject)
