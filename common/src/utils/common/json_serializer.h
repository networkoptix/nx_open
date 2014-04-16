#ifndef QN_JSON_SERIALIZER_H
#define QN_JSON_SERIALIZER_H

#include <QtCore/QMetaType>

//#include <>

class QnJsonContext;

class QnJsonSerializer {
public:
    QnJsonSerializer(int type): m_type(type) {}
    virtual ~QnJsonSerializer() {}

    static QnJsonSerializer *forType(int type);
    static QList<QnJsonSerializer *> allSerializers();
    static void registerSerializer(QnJsonSerializer *serializer);
    
    template<class T>
    static void registerSerializer();

    int type() const {
        return m_type;
    }

    void serialize(QnJsonContext *ctx, const QVariant &value, QJsonValue *target) const {
        assert(value.userType() == m_type && target);

        serializeInternal(ctx, value.constData(), target);
    }

    void serialize(QnJsonContext *ctx, const void *value, QJsonValue *target) const {
        assert(value && target);

        serializeInternal(ctx, value, target);
    }

    bool deserialize(QnJsonContext *ctx, const QJsonValue &value, QVariant *target) const {
        assert(target);

        *target = QVariant(m_type, static_cast<const void *>(NULL));
        return deserializeInternal(ctx, value, target->data());
    }

    bool deserialize(QnJsonContext *ctx, const QJsonValue &value, void *target) const {
        assert(target);

        return deserializeInternal(ctx, value, target);
    }

protected:
    virtual void serializeInternal(QnJsonContext *ctx, const void *value, QJsonValue *target) const = 0;
    virtual bool deserializeInternal(QnJsonContext *ctx, const QJsonValue &value, void *target) const = 0;

private:
    int m_type;
};


namespace QJsonDetail {
    template<class T>
    void serialize_value_direct(QnJsonContext *ctx, const T &value, QJsonValue *target);
    template<class T>
    bool deserialize_value_direct(QnJsonContext *ctx, const QJsonValue &value, T *target);
} // namespace QJsonDetail


template<class T>
class QnDefaultJsonSerializer: public QnJsonSerializer {
public:
    QnDefaultJsonSerializer(): QnJsonSerializer(qMetaTypeId<T>()) {}

protected:
    virtual void serializeInternal(QnJsonContext *ctx, const void *value, QJsonValue *target) const override {
        QJsonDetail::serialize_value_direct(ctx, *static_cast<const T *>(value), target);
    }

    virtual bool deserializeInternal(QnJsonContext *ctx, const QJsonValue &value, void *target) const override {
        return QJsonDetail::deserialize_value_direct(ctx, value, static_cast<T *>(target));
    }
};


template<class T>
void QnJsonSerializer::registerSerializer() {
    registerSerializer(new QnDefaultJsonSerializer<T>());
}

#include "json.h"

#endif // QN_JSON_SERIALIZER_H
