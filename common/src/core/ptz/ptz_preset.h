#ifndef QN_PTZ_PRESET_H
#define QN_PTZ_PRESET_H

#include <QtCore/QString>
#include <QtCore/QList>

#include "ptz_fwd.h"

class QnPtzPreset {
public:
    QnPtzPreset() {}
    QnPtzPreset(const QString &id, const QString &name): m_id(id), m_name(name) {}

    bool isNull() const {
        return m_id.isNull();
    }

    const QString &id() const {
        return m_id;
    }

    const QString &name() const {
        return m_name;
    }

private:
    QString m_id, m_name;
};


#endif // QN_PTZ_PRESET_H
