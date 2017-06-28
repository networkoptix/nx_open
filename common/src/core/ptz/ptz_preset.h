#ifndef QN_PTZ_PRESET_H
#define QN_PTZ_PRESET_H

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMetaType>

#ifndef Q_MOC_RUN
#include <boost/operators.hpp>
#endif

#include "ptz_fwd.h"

struct QnPtzPreset: public boost::equality_comparable1<QnPtzPreset>
{
    Q_GADGET
public:
    QnPtzPreset() {}
    QnPtzPreset(const QString &id, const QString &name): id(id), name(name) {}

    friend bool operator==(const QnPtzPreset &l, const QnPtzPreset &r);

    QString id;
    QString name;
};
#define QnPtzPreset_Fields (id)(name)

Q_DECLARE_METATYPE(QnPtzPreset)
Q_DECLARE_METATYPE(QnPtzPresetList)

#endif // QN_PTZ_PRESET_H
