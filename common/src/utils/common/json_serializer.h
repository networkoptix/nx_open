#ifndef QN_JSON_SERIALIZER_H
#define QN_JSON_SERIALIZER_H

#include <QtCore/QMetaType>

#include "json.h"

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

    void serialize(const QVariant &value, QJsonValue *target) const {
        assert(value.userType() == m_type && target);

        serializeInternal(value.constData(), target);
    }

    void serialize(const void *value, QJsonValue *target) const {
        assert(value && target);

        serializeInternal(value, target);
    }

    bool deserialize(const QJsonValue &value, QVariant *target) const {
        assert(target);

        *target = QVariant(m_type, static_cast<const void *>(NULL));
        return deserializeInternal(value, target->data());
    }

    bool deserialize(const QJsonValue &value, void *target) const {
        assert(target);

        return deserializeInternal(value, target);
    }

protected:
    virtual void serializeInternal(const void *value, QJsonValue *target) const = 0;
    virtual bool deserializeInternal(const QJsonValue &value, void *target) const = 0;

private:
    int m_type;
};


template<class T>
class QnDefaultJsonSerializer: public QnJsonSerializer {
public:
    QnDefaultJsonSerializer(): QnJsonSerializer(qMetaTypeId<T>()) {}

protected:
    virtual void serializeInternal(const void *value, QJsonValue *target) const override {
        QJson::serialize(*static_cast<const T *>(value), target);
    }

    virtual bool deserializeInternal(const QJsonValue &value, void *target) const override {
        return QJson::deserialize(value, static_cast<T *>(target));
    }
};


template<class T>
void QnJsonSerializer::registerSerializer() {
    registerSerializer(new QnDefaultJsonSerializer<T>());
}


#endif // QN_JSON_SERIALIZER_H
