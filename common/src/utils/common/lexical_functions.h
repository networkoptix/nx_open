#ifndef QN_LEXICAL_FUNCTIONS_H
#define QN_LEXICAL_FUNCTIONS_H

#include <QtCore/QUuid>

#include "lexical.h"

namespace QnLexicalDetail {
    template<class T, class Numeric>
    void serialize_numeric(const T &value, QString *target) {
        *target = QString::number(static_cast<Numeric>(value));
    }

    template<class T, class Numeric>
    bool deserialize_numeric(const QString &value, T *target) {
        Numeric tmp;
        if(!QnLexical::deserialize(value, &tmp))
            return false;
        if(tmp < static_cast<Numeric>(std::numeric_limits<T>::min()) || tmp > static_cast<Numeric>(std::numeric_limits<T>::max()))
            return false;

        *target = static_cast<T>(tmp);
        return true;
    }

} // namespace QnLexicalDetail


/* Free-standing (de)serialization functions are picked up via ADL by
 * the actual implementation. Feel free to add them for your own types. */


inline void serialize(const QString &value, QString *target) {
    *target = value;
}

inline bool deserialize(const QString &value, QString *target) {
    *target = value;
    return true;
}


#define QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(TYPE, TYPE_GETTER, ... /* NUMBER_FORMAT */) \
inline void serialize(const TYPE &value, QString *target) {                     \
    *target = QString::number(value, ##__VA_ARGS__);                            \
}                                                                               \
                                                                                \
inline bool deserialize(const QString &value, TYPE *target) {                   \
    bool ok = false;                                                            \
    TYPE result = value.TYPE_GETTER(&ok);                                       \
    if(ok)                                                                      \
        *target = result;                                                       \
    return result;                                                              \
}

QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(int,                   toInt)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(unsigned int,          toUInt)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(long,                  toLong)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(unsigned long,         toULong)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(long long,             toLongLong)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(unsigned long long,    toULongLong)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(float,                 toFloat, 'g', 9)
QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS(double,                toDouble, 'g', 17)
#undef QN_DEFINE_DIRECT_LEXICAL_SERIALIZATION_FUNCTIONS


#define QN_DEFINE_NUMERIC_CONVERSION_LEXICAL_SERIALIZATION_FUNCTIONS(TYPE)      \
inline void serialize(const TYPE &value, QString *target) {                     \
    QnLexicalDetail::serialize_numeric<TYPE, int>(value, target);               \
}                                                                               \
                                                                                \
inline bool deserialize(const QString &value, TYPE *target) {                   \
    return QnLexicalDetail::deserialize_numeric<TYPE, int>(value, target);      \
}

QN_DEFINE_NUMERIC_CONVERSION_LEXICAL_SERIALIZATION_FUNCTIONS(char)
QN_DEFINE_NUMERIC_CONVERSION_LEXICAL_SERIALIZATION_FUNCTIONS(unsigned char)
QN_DEFINE_NUMERIC_CONVERSION_LEXICAL_SERIALIZATION_FUNCTIONS(short)
QN_DEFINE_NUMERIC_CONVERSION_LEXICAL_SERIALIZATION_FUNCTIONS(unsigned short)
#undef QN_DEFINE_NUMERIC_CONVERSION_LEXICAL_SERIALIZATION_FUNCTIONS


QN_DECLARE_LEXICAL_SERIALIZATION_FUNCTIONS(bool)
QN_DECLARE_LEXICAL_SERIALIZATION_FUNCTIONS(QUuid)



#endif // QN_LEXICAL_FUNCTIONS_H
