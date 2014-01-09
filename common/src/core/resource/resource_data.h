#ifndef QN_RESOURCE_DATA_H
#define QN_RESOURCE_DATA_H

#include <QtCore/QHash>
#include <QtCore/QMetaType>

#include <core/ptz/ptz_fwd.h>

class QnResourceData {
public:
    QnResourceData() {}

    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const {
        return m_valueByKey.value(key, defaultValue);
    }

    template<class T>
    T value(const QString &key, const T &defaultValue = T()) const {
        auto pos = m_valueByKey.find(key);
        if(pos == m_valueByKey.end())
            return defaultValue;

        QVariant result = *pos;
        if(!result.convert(static_cast<QVariant::Type>(qMetaTypeId<T>())))
            return defaultValue;

        return result.value<T>();
    }

    void setValue(const QString &key, const QVariant &value) {
        m_valueByKey.insert(key, value);
    }

    template<class T>
    void setValue(const QString &key, const T &value) {
        m_valueByKey.insert(key, QVariant::fromValue<T>(value));
    }

    void addData(const QnResourceData &other) {
        if(m_valueByKey.isEmpty()) {
            m_valueByKey = other.m_valueByKey;
        } else {
            for(auto pos = other.m_valueByKey.begin(); pos != other.m_valueByKey.end(); pos++)
                m_valueByKey.insert(pos.key(), pos.value());
        }
    }

    /* Built-in data follows. */

    QnPtzMapperPtr ptzMapper();

private:
#ifdef _DEBUG
    QMap<QString, QVariant> m_valueByKey; /* Map is easier to look through in debug. */
#else
    QHash<QString, QVariant> m_valueByKey;
#endif
};

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnResourceData *target);

#endif // QN_RESOURCE_DATA_H
