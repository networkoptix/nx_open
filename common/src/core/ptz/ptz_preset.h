#ifndef QN_PTZ_PRESET_H
#define QN_PTZ_PRESET_H

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMetaType>

#include <boost/operators.hpp>

#include "ptz_fwd.h"
#include "utils/math/math.h"

struct QnPtzPreset: public boost::equality_comparable1<QnPtzPreset> {
public:
    QnPtzPreset() {}
    QnPtzPreset(const QString &id, const QString &name): id(id), name(name), space(Qn::DeviceCoordinateSpace), position(qQNaN<QVector3D>()) {}

    friend bool operator==(const QnPtzPreset &l, const QnPtzPreset &r);

    QString id;
    QString name;
    Qn::PtzCoordinateSpace space;
    QVector3D position;
};

Q_DECLARE_METATYPE(QnPtzPreset)
Q_DECLARE_METATYPE(QnPtzPresetList)

#endif // QN_PTZ_PRESET_H
