#ifndef QN_PTZ_PRESET_H
#define QN_PTZ_PRESET_H

#include <QtCore/QString>
#include <QtCore/QList>

#include "ptz_fwd.h"

#include <utils/common/json.h>

class QnPtzPreset {
public:
    QnPtzPreset() {}
    QnPtzPreset(const QString &id, const QString &name): m_id(id), m_name(name) {}

    bool isNull() const { return m_id.isNull(); }

    const QString &id() const { return m_id; }
    void setId(const QString &id) { m_id = id; }

    const QString &name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

private:
    QString m_id, m_name;
};

QN_DEFINE_CLASS_JSON_SERIALIZATION_FUNCTIONS(QnPtzPreset,
    ((&QnPtzPreset::id,   &QnPtzPreset::setId,     "id"))
    ((&QnPtzPreset::name, &QnPtzPreset::setName,   "name")),
inline)


#endif // QN_PTZ_PRESET_H
