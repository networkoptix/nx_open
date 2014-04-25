#ifndef QN_LEXICAL_H
#define QN_LEXICAL_H

#include <cassert>

#include <QtCore/QString>

#include "adl_wrapper.h"
#include "lexical_fwd.h"

namespace QnLexicalDetail {
    template<class T>
    void serialize_value(const T &value, QString *target) {
        serialize(value, target); /* That's the place where ADL kicks in. */
    }

    template<class T>
    bool deserialize_value(const QString &value, T *target) {
        /* That's the place where ADL kicks in.
         * 
         * Note that we wrap a string into a wrapper so that
         * ADL would find only overloads with QString as the first parameter. 
         * Otherwise other overloads could be discovered. */
        return deserialize(adlWrap(value), target);
    }

} // namespace QnLexicalDetail


namespace QnLexical {
    template<class T>
    void serialize(const T &value, QString *target) {
        assert(target);

        QnLexicalDetail::serialize_value(value, target);
    }

    template<class T>
    bool deserialize(const QString &value, T *target) {
        assert(target);

        return QnLexicalDetail::deserialize_value(value, target);
    }

    template<class T>
    QString serialized(const T &value) {
        QString result;
        QnLexical::serialize(value, &result);
        return result;
    }

    template<class T>
    T deserialized(const QString &value, const T &defaultValue, bool *success = NULL) {
        T target;
        bool result = QnLexical::deserialize(value, &target);
        if (success)
            *success = result;
        return result ? target : defaultValue;
    }

} // namespace QnLexical


#define QN_DEFINE_ENUM_CAST_LEXICAL_SERIALIZATION_FUNCTIONS(TYPE, ... /* PREFIX */) \
__VA_ARGS__ void serialize(const TYPE &value, QString *target) {                \
    QnLexical::serialize(static_cast<int>(value), target);                      \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(const QString &value, TYPE *target) {              \
    int intValue;                                                               \
    if(!QnLexical::deserialize(value, &intValue))                               \
        return false;                                                           \
                                                                                \
    *target = static_cast<TYPE>(intValue);                                      \
    return true;                                                                \
}


#define QN_DEFINE_ENUM_MAPPED_LEXICAL_SERIALIZATION_FUNCTIONS(TYPE, ... /* PREFIX */)  \
    QN_DEFINE_ENUM_MAPPED_LEXICAL_SERIALIZATION_FUNCTIONS_I(TYPE, BOOST_PP_CAT(qn_typedEnumNameMapper_instance, __LINE__), ##__VA_ARGS__)

#define QN_DEFINE_ENUM_MAPPED_LEXICAL_SERIALIZATION_FUNCTIONS_I(TYPE, STATIC_NAME, ... /* PREFIX */)  \
template<class T> void QN_DEFINE_ENUM_LEXICAL_SERIALIZATION_FUNCTIONS_macro_cannot_be_used_in_header_files(); \
template<> void QN_DEFINE_ENUM_LEXICAL_SERIALIZATION_FUNCTIONS_macro_cannot_be_used_in_header_files<TYPE>() {}; \
                                                                                \
Q_GLOBAL_STATIC_WITH_ARGS(QnTypedEnumNameMapper<TYPE>, STATIC_NAME, (QnEnumNameMapper::create<TYPE>())) \
                                                                                \
__VA_ARGS__ void serialize(const TYPE &value, QString *target) {                \
    *target = STATIC_NAME()->name(value);                                       \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(const QString &value, TYPE *target) {              \
    bool ok;                                                                    \
    TYPE result = STATIC_NAME()->value(value, &ok);                             \
    if(!ok) {                                                                   \
        return false;                                                           \
    } else {                                                                    \
        *target = result;                                                       \
        return true;                                                            \
    }                                                                           \
}

#include "lexical_functions.h"

#endif // QN_LEXICAL_H
