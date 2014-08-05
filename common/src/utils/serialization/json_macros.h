#ifndef QN_SERIALIZATION_JSON_MACROS_H
#define QN_SERIALIZATION_JSON_MACROS_H

#include <utils/fusion/fusion_serialization.h>

#include "json.h"
#include "lexical.h"

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
    
#endif // QN_SERIALIZATION_JSON_MACROS_H
