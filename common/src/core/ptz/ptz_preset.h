#ifndef QN_PTZ_PRESET_H
#define QN_PTZ_PRESET_H

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMetaType>

#include "ptz_fwd.h"

struct QnPtzPreset {
public:
    QnPtzPreset() {}
    QnPtzPreset(const QString &id, const QString &name): id(id), name(name) {}

    QString id;
    QString name;
};

Q_DECLARE_METATYPE(QnPtzPreset)
Q_DECLARE_METATYPE(QnPtzPresetList)

#endif // QN_PTZ_PRESET_H
