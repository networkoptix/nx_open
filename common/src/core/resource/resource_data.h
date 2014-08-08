#ifndef QN_RESOURCE_DATA_H
#define QN_RESOURCE_DATA_H

#include <QtCore/QHash>
#include <QtCore/QMetaType>
#include <QtCore/QJsonValue>

#include <utils/common/warnings.h>

class QnResourceData {
public:
    QnResourceData() {}

    template<class T>
    bool value(const QString &key, T *value) const {
        return this->value(
            key, 
            qMetaTypeId<T>(), 
            value, 
            [](const void *src, void *dst) { *static_cast<T *>(dst) = *static_cast<const T *>(src); }
        );
    }

    template<class T>
    T value(const QString &key, const T &defaultValue = T()) const {
        T result;
        return this->value(key, &result) ? result : defaultValue;
    }

    void add(const QnResourceData &other);

private:
    typedef void (*CopyFunction)(const void *src, void *dst);

    bool value(const QString &key, int type, void *value, const CopyFunction &copyFunction) const;

private:
    friend class QnResourceDataJsonSerializer;

    struct Data {
        Data(): type(QMetaType::UnknownType) {}

        int type;
        QJsonValue json;
        QVariant value;
    };

#ifdef _DEBUG
    QMap<QString, Data> m_dataByKey; /* Map is easier to look through in debug. */
#else
    QHash<QString, Data> m_dataByKey;
#endif
};

Q_DECLARE_METATYPE(QnResourceData)

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnResourceData *target);

#endif // QN_RESOURCE_DATA_H
