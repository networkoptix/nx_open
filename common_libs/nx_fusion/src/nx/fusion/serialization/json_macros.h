#ifndef QN_SERIALIZATION_JSON_MACROS_H
#define QN_SERIALIZATION_JSON_MACROS_H

#include <typeinfo>

#include <nx/fusion/fusion/fusion_serialization.h>
#include <nx/utils/unused.h>

#include "json.h"
#include "lexical.h"

namespace QJsonDetail {

/** Return true if the whole struct must be serialized. Is not used for any structs really. */
struct TrueChecker
{
    template<class T>
    bool operator()(const T &) const { return true; }
};

/** Return true if value must be omitted in lazy mode. */
struct LazyChecker
{
    template<class T>
    bool operator()(const T &) const { return false; }

    bool operator()(const QString& value) const { return value.isEmpty(); }
    bool operator()(const QnUuid& value) const { return value.isNull(); }
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

            // In lazy mode we can skip empty structure fields
            if (access(lazy, false))
            {
                const auto realValue = invoke(access(getter), value);
                if (LazyChecker()(realValue))
                    return true;
            }

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

    struct DeserializationVisitor
    {
    public:
        DeserializationVisitor(QnJsonContext* ctx, const QJsonValue& value):
            m_ctx(ctx),
            m_value(value),
            m_object(value.toObject())
        {
        }

        template<class T, class Access>
        bool operator()(const T&, const Access&, const QnFusion::start_tag&)
        {
            return m_value.isObject();
        }

        template<class T, class Access>
        bool operator()(T& target, const Access& access)
        {
            using namespace QnFusion;

            return operator()(target, access, access(setter_tag));
        }

    private:
        /**
         * Retrieve map of deprecated field names, if it is available. If T provides
         * getDeprecatedFields(), return its result, otherwise, return null.
         * Sfinae wrapper.
         */
        template<class T>
        static DeprecatedFieldNames* getDeprecatedFieldNames(const T& target)
        {
            return getDeprecatedFieldNamesSfinae(target, /*enable_if_member_exists*/ nullptr);
        }

        /**
         * Sfinae: Called when T provides getDeprecatedFields().
         */
        template<class T>
        static DeprecatedFieldNames* getDeprecatedFieldNamesSfinae(const T& target,
            decltype(&T::getDeprecatedFieldNames) /*enable_if_member_exists*/)
        {
            QN_UNUSED(target); //< Suppress inappropriate MSVC warning C4100.
            return target.getDeprecatedFieldNames();
        }

        /**
         * Sfinae: Called when T does not provide getDeprecatedFields().
         */
        template<class T>
        static DeprecatedFieldNames* getDeprecatedFieldNamesSfinae(const T& /*target*/,
            ... /*enable_if_member_exists*/)
        {
            return nullptr;
        }

        template<class T, class Access>
        bool operator()(T& target, const Access& access,
            const QnFusion::member_setter_tag&)
        {
            using namespace QnFusion;

            bool found = false;
            if (!QJson::deserialize(
                m_ctx, m_object, access(name), &(target.*access(setter)), access(optional, true),
                &found, getDeprecatedFieldNames(target), typeid(target)))
            {
                return false;
            }
            if (!found)
                m_ctx->setSomeFieldsNotFound(true);
            return true;
        }

        template<class T, class Access, class Member>
        bool operator()(T& target, const Access& access,
            const QnFusion::typed_function_setter_tag<Member>&)
        {
            using namespace QnFusion;

            bool found = false;
            Member member;
            if (!QJson::deserialize(
                m_ctx, m_object, access(name), &member, access(optional, true),
                &found, getDeprecatedFieldNames(target), typeid(target)))
            {
                return false;
            }
            if (found)
                invoke(access(setter), target, std::move(member));
            else
                m_ctx->setSomeFieldsNotFound(true);
            return true;
        }

    private:
        QnJsonContext* m_ctx;
        const QJsonValue& m_value;
        QJsonObject m_object;
    };

} // namespace QJsonDetail

#if defined(_MSC_VER)
/* Workaround against boost bug in variadic templates implementation, introduced in Boost 1.57. */
#pragma warning(disable:4003)
#endif
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
