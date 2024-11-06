// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_RESOURCE_DATA_H
#define QN_RESOURCE_DATA_H

#include <QtCore/QHash>
#include <QtCore/QJsonValue>
#include <QtCore/QVariant>

class QnJsonContext;

class NX_VMS_COMMON_API QnResourceData
{
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

    bool contains(const QString &key) const
    {
        return m_dataByKey.contains(key);
    }

    void clearKeyTags();

    QJsonObject allValuesRaw() const;

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

bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QnResourceData *target);

#endif // QN_RESOURCE_DATA_H
