#ifndef QN_RESOURCE_DATA_H
#define QN_RESOURCE_DATA_H

#include <QtCore/QHash>
#include <QtCore/QMetaType>

#include <core/ptz/ptz_fwd.h>

class QnResourceData {
    typedef QHash<QString, QVariant> DataHash;

public:
    QnResourceData() {}

    template<class T>
    T value(const QString &key, const T &defaultValue = T()) {
        DataHash::const_iterator pos = m_valueByKey.find(key);
        if(pos == m_valueByKey.end())
            return defaultValue;

        QVariant result = *pos;
        if(!result.convert(static_cast<QVariant::Type>(qMetaTypeId<T>())))
            return defaultValue;

        return result.value<T>();
    }

    template<class T>
    void setValue(const QString &key, const T &value) {
        m_valueByKey.insert(key, QVariant::fromValue<T>(value));
    }

    void addData(const QnResourceData &other) {
        if(m_valueByKey.isEmpty()) {
            m_valueByKey = other.m_valueByKey;
        } else {
            for(DataHash::const_iterator pos = other.m_valueByKey.begin(); pos != other.m_valueByKey.end(); pos++)
                m_valueByKey.insert(pos.key(), pos.value());
        }
    }

    /* Built-in data follows. */

    QnPtzMapperPtr ptzMapper();

private:
    QHash<QString, QVariant> m_valueByKey;
};

bool deserialize(const QJsonValue &value, QnResourceData *target);

#endif // QN_RESOURCE_DATA_H
