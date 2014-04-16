#ifndef QN_LEXICAL_H
#define QN_LEXICAL_H

#include <cassert>

#include <QtCore/QString>

#include "adl_wrapper.h"
#include "lexical_fwd.h"

#include <utils/serialization/serialization.h>
#include <utils/serialization/serialization_functions.h>


namespace QnLexical {
    template<class T>
    void serialize(const T &value, QString *target) {
        Qss::serialize(value, target);
    }

    template<class T>
    bool deserialize(const QString &value, T *target) {
        return Qss::deserialize(value, target);
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
