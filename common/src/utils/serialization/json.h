#ifndef QN_SERIALIZATION_JSON_H
#define QN_SERIALIZATION_JSON_H

#include <cassert>
#include <limits>

#ifndef Q_MOC_RUN
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/is_function.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <boost/type_traits/remove_const.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/mpl/bool.hpp>
#endif // Q_MOC_RUN

#include <QtCore/QJsonValue>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>

#include <utils/fusion/fusion_serialization.h>
#include <utils/serialization/serialization.h>

#include "json_fwd.h"
#include "lexical.h"

class QnJsonSerializer;

namespace QJsonDetail {
    void serialize_json(const QJsonValue &value, QByteArray *target, QJsonDocument::JsonFormat format = QJsonDocument::Compact);
    bool deserialize_json(const QByteArray &value, QJsonValue *target);

    struct StorageInstance { 
        QnSerializerStorage<QnJsonSerializer> *operator()() const;
    };
} // namespace QJsonDetail


class QnJsonContext: public QnSerializationContext<QnJsonSerializer> {};

class QnJsonSerializer: public QnContextSerializer<QJsonValue, QnJsonContext>, public QnStaticSerializerStorage<QnJsonSerializer, QJsonDetail::StorageInstance> {
    typedef QnContextSerializer<QJsonValue, QnJsonContext> base_type;
public:
    QnJsonSerializer(int type): base_type(type) {}
};

template<class T>
class QnDefaultJsonSerializer: public QnDefaultContextSerializer<T, QnJsonSerializer> {};


namespace QJson {
    /**
     * Serializes the given value into intermediate JSON representation.
     * 
     * \param ctx                       JSON context to use.
     * \param value                     Value to serialize.
     * \param[out] target               Target JSON value, must not be NULL.
     */
    template<class T>
    void serialize(QnJsonContext *ctx, const T &value, QJsonValue *target) {
        QnSerialization::serialize(ctx, value, target);
    }

    template<class T>
    void serialize(QnJsonContext *ctx, const T &value, QJsonValueRef *target) {
        assert(target);

        QJsonValue jsonValue;
        QnSerialization::serialize(ctx, value, &jsonValue);
        *target = jsonValue;
    }

    template<class T>
    void serialize(QnJsonContext *ctx, const T &value, const QString &key, QJsonObject *target) {
        assert(target);

        QJsonValueRef jsonValue = (*target)[key];
        QJson::serialize(ctx, value, &jsonValue);
    }

    /**
     * Serializes the given value into a JSON string.
     * 
     * \param ctx                       JSON context to use.
     * \param value                     Value to serialize.
     * \param[out] target               Target JSON string, must not be NULL.
     */
    template<class T>
    void serialize(QnJsonContext *ctx, const T &value, QByteArray *target) {
        assert(target);

        QJsonValue jsonValue;
        QJson::serialize(ctx, value, &jsonValue);
        QJsonDetail::serialize_json(jsonValue, target);
    }

    template<class T>
    void serialize(const T &value, QJsonValue *target) {
        QnJsonContext ctx;
        QJson::serialize(&ctx, value, target);
    }

    template<class T>
    void serialize(const T &value, QJsonValueRef *target) {
        QnJsonContext ctx;
        QJson::serialize(&ctx, value, target);
    }

    template<class T>
    void serialize(const T &value, const QString &key, QJsonObject *target) {
        QnJsonContext ctx;
        QJson::serialize(&ctx, value, key, target);
    }

    template<class T>
    void serialize(const T &value, QByteArray *target) {
        QnJsonContext ctx;
        QJson::serialize(&ctx, value, target);
    }


    /**
     * Deserializes the given intermediate representation of a JSON object.
     * Note that <tt>boost::enable_if</tt> is used to prevent implicit conversions
     * in the first argument.
     * 
     * \param ctx                       JSON context to use.
     * \param value                     Intermediate JSON representation to deserialize.
     * \param[out] target               Deserialization target, must not be NULL.
     * \returns                         Whether the deserialization was successful.
     */
    template<class T, class QJsonValue>
    bool deserialize(QnJsonContext *ctx, const QJsonValue &value, T *target, typename boost::enable_if<boost::is_same<QJsonValue, ::QJsonValue> >::type * = NULL) {
        return QnSerialization::deserialize(ctx, value, target);
    }

    template<class T>
    bool deserialize(QnJsonContext *ctx, const QJsonValueRef &value, T *target) {
        return QnSerialization::deserialize(ctx, static_cast<QJsonValue>(value), target);
    }

    template<class T>
    bool deserialize(QnJsonContext *ctx, const QJsonObject &value, const QString &key, T *target, bool optional = false, bool *found = NULL) {
        QJsonObject::const_iterator pos = value.find(key);
        if(pos == value.end()) {
            if(found)
                *found = false;
            return optional;
        } else {
            if(found)
                *found = true;
            return QJson::deserialize(ctx, *pos, target);
        }
    }

    /**
     * Deserializes a value from a JSON string.
     * 
     * \param ctx                       JSON context to use.
     * \param value                     JSON string to deserialize.
     * \param[out] target               Deserialization target, must not be NULL.
     * \returns                         Whether the deserialization was successful.
     */
    template<class T>
    bool deserialize(QnJsonContext *ctx, const QByteArray &value, T *target) {
        assert(target);

        QJsonValue jsonValue;
        if(!QJsonDetail::deserialize_json(value, &jsonValue))
            return false;
        return QJson::deserialize(ctx, jsonValue, target);
    }

    template<class T, class QJsonValue>
    bool deserialize(const QJsonValue &value, T *target, typename boost::enable_if<boost::is_same<QJsonValue, ::QJsonValue> >::type * = NULL) {
        QnJsonContext ctx;
        return QJson::deserialize(&ctx, value, target);
    }

    template<class T>
    bool deserialize(const QJsonValueRef &value, T *target) {
        QnJsonContext ctx;
        return QJson::deserialize(&ctx, value, target);
    }

    template<class T>
    bool deserialize(const QJsonObject &value, const QString &key, T *target, bool optional = false) {
        QnJsonContext ctx;
        return QJson::deserialize(&ctx, value, key, target, optional);
    }

    template<class T>
    bool deserialize(const QByteArray &value, T *target) {
        QnJsonContext ctx;
        return QJson::deserialize(&ctx, value, target);
    }


    /**
     * Serializes the given value into a JSON string and returns it in the utf-8 format.
     *
     * \param value                     Value to serialize.
     * \returns                         Result JSON data.
     */
    template<class T>
    QByteArray serialized(const T &value) {
        QByteArray result;
        QJson::serialize(value, &result);
        return result;
    }

    /**
     * Deserializes a value from a JSON utf-8-encoded string.
     *
     * \param value                     JSON string to deserialize.
     * \param defaultValue              Value to return in case of deserialization failure.
     * \param[out] success              Deserialization status.
     * \returns                         Deserialization target.
     */
    template<class T>
    T deserialized(const QByteArray &value, const T &defaultValue = T(), bool *success = NULL) {
        T target;
        bool result = QJson::deserialize(value, &target);
        if (success)
            *success = result;
        return result ? target : defaultValue;
    }

} // namespace QJson


namespace QJsonDetail {
    struct TrueChecker {
        template<class T>
        bool operator()(const T &) const { return true; }
    };

    class SerializationVisitor {
    public:
        SerializationVisitor(QnJsonContext *ctx, QJsonValue &target): 
            m_ctx(ctx), 
            m_target(target) 
        {}

        template<class T, class Access>
        bool operator()(const T &value, const Access &access) {
            using namespace QnFusion;

            if(!invoke(access(checker, TrueChecker()), value))
                return true; /* Skipped. */

            QJson::serialize(m_ctx, invoke(access(getter), value), access(name), &m_object);
            return true;
        }

        ~SerializationVisitor() {
            m_target = std::move(m_object);
        }

    private:
        QnJsonContext *m_ctx;
        QJsonValue &m_target;
        QJsonObject m_object;
    };

    struct DeserializationVisitor {
    public:
        DeserializationVisitor(QnJsonContext *ctx, const QJsonValue &value):
            m_ctx(ctx),
            m_value(value),
            m_object(value.toObject())
        {}

        template<class T, class Access>
        bool operator()(const T &, const Access &, const QnFusion::start_tag &) {
            return m_value.isObject();
        }

        template<class T, class Access>
        bool operator()(T &target, const Access &access) {
            using namespace QnFusion;

            return operator()(target, access, access(setter_tag));
        }

    private:
        template<class T, class Access>
        bool operator()(T &target, const Access &access, const QnFusion::member_setter_tag &) {
            using namespace QnFusion;

            return QJson::deserialize(m_ctx, m_object, access(name), &(target.*access(setter)), access(optional, false));
        }

        template<class T, class Access, class Member>
        bool operator()(T &target, const Access &access, const QnFusion::typed_function_setter_tag<Member> &) {
            using namespace QnFusion;

            bool found = false;
            Member member;
            if(!QJson::deserialize(m_ctx, m_object, access(name), &member, access(optional, false), &found))
                return false;
            if(found)
                invoke(access(setter), target, std::move(member));
            return true;
        }

    private:
        QnJsonContext *m_ctx;
        const QJsonValue &m_value;
        QJsonObject m_object;
    };

} // namespace QJsonDetail


QN_FUSION_REGISTER_SERIALIZATION_VISITORS(QJsonValue, QJsonDetail::SerializationVisitor, QJsonDetail::DeserializationVisitor)


#define QN_FUSION_DEFINE_FUNCTIONS_json_lexical(TYPE, ... /* PREFIX */)         \
__VA_ARGS__ void serialize(QnJsonContext *, const TYPE &value, QJsonValue *target) { \
    *target = QnLexical::serialized(value);                                     \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(QnJsonContext *, const QJsonValue &value, TYPE *target) { \
    QString string;                                                             \
    return QJson::deserialize(value, &string) && QnLexical::deserialize(string, target); \
}


#define QN_FUSION_DEFINE_FUNCTIONS_json(TYPE, ... /* PREFIX */)                 \
__VA_ARGS__ void serialize(QnJsonContext *ctx, const TYPE &value, QJsonValue *target) { \
    QnFusion::serialize(ctx, value, target);                                    \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(QnJsonContext *ctx, const QJsonValue &value, TYPE *target) { \
    return QnFusion::deserialize(ctx, value, target);                           \
}


#endif // QN_SERIALIZATION_JSON_H
