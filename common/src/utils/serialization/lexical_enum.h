#ifndef QN_LEXICAL_ENUM_H
#define QN_LEXICAL_ENUM_H

#include <cassert>

#include <type_traits> /* For std::integral_constant, std::declval, std::enable_if. */
#include <utility> /* For std::move. */

#include <QtCore/QHash>
#include <QtCore/QString>

#include <utils/common/type_traits.h>

#include "lexical.h"
#include "enum.h"

struct QMetaObject;

namespace QnLexicalDetail {
    using namespace QnTypeTraits;

    template<class T>
    yes_type lexical_numeric_check_test(const T &, const decltype(lexical_numeric_check(std::declval<const T *>())) * = NULL);
    no_type lexical_numeric_check_test(...);

    template<class T>
    struct is_numerically_serializable:
        std::integral_constant<bool, sizeof(lexical_numeric_check_test(std::declval<T>())) == sizeof(yes_type)>
    {};


    inline bool isNullString(const char *s) { return s == NULL; }
    inline bool isNullString(const QString &s) { return s.isNull(); }

} // namespace QnLexicalDetail


class QnEnumLexicalSerializerData {
public:
    QnEnumLexicalSerializerData(): m_caseSensitivity(Qt::CaseSensitive), m_flagged(false), m_numeric(false) {}

    void load(const QMetaObject *metaObject, const char *enumName);

    void insert(int value, const QString &name);
    void insert(int value, const char *name);
    void clear();

    bool isFlagged() const;
    void setFlagged(bool flagged);

    bool isNumeric() const;
    void setNumeric(bool numeric);

    Qt::CaseSensitivity caseSensitivity() const;
    void setCaseSensitivity(Qt::CaseSensitivity caseSensitivity);

    void serialize(int value, QString *target) const;
    bool deserialize(const QString &value, int *target) const;

    template<class T>
    void serialize(T value, QString *target) const {
        serialize(static_cast<int>(value), target);
    }

    template<class T>
    bool deserialize(const QString &value, T *target) const {
        int tmp;
        if(!deserialize(value, &tmp))
            return false;

        *target = static_cast<T>(tmp);
        return true;
    }

protected:
    void serializeEnum(int value, QString *target) const;
    bool deserializeEnum(const QString &value, int *target) const;
    bool deserializeEnumInternal(const QHash<QString, int> &hash, const QString &value, int *target) const;

    void serializeFlags(int value, QString *target) const;
    bool deserializeFlags(const QString &value, int *target) const;

private:
    QHash<int, QString> m_nameByValue;
    QHash<QString, int> m_valueByName;
    QHash<QString, int> m_valueByLowerName;
    QString m_enumName;
    Qt::CaseSensitivity m_caseSensitivity;
    bool m_flagged;
    bool m_numeric;
};


template<class T>
class QnEnumLexicalSerializer: public QnLexicalSerializer {
    static_assert(sizeof(T) <= sizeof(int), "Enumeration types larger than int in size are not supported.");
    typedef QnLexicalSerializer base_type;

public:
    QnEnumLexicalSerializer(): 
        base_type(qMetaTypeId<T>())
    {}

    QnEnumLexicalSerializer(const QnEnumLexicalSerializerData &data): 
        base_type(qMetaTypeId<T>()),
        m_data(data)
    {}

protected:
    virtual void serializeInternal(const void *value, QString *target) const override {
        m_data.serialize(*static_cast<const T *>(value), target);
    }

    virtual bool deserializeInternal(const QString &value, void *target) const override {
        return m_data.deserialize(value, static_cast<T *>(target));
    }

private:
    QnEnumLexicalSerializerData m_data;
};


namespace QnLexical {
    template<class T, class Integer>
    QnEnumLexicalSerializer<Integer> *newEnumSerializer() {
        return new QnEnumLexicalSerializer<Integer>(create_enum_lexical_serializer_data(static_cast<const T *>(NULL)));
    }

    template<class T>
    QnEnumLexicalSerializer<T> *newEnumSerializer() {
        return newEnumSerializer<T, T>();
    }

    /**
     * This metafunction returns whether the lexical serialization functions
     * for the given enum type use numeric serialization, i.e. serialize
     * it as a number.
     */
    template<class T>
    struct is_numerically_serializable:
        QnLexicalDetail::is_numerically_serializable<T>
    {};

    template<class T>
    struct is_numerically_serializable_enum_or_flags:
        std::integral_constant<
            bool, 
            is_numerically_serializable<T>::value && QnSerialization::is_enum_or_flags<T>::value
        >
    {};


} // namespace QnLexical


/**
 * This macro is here just for the uniformity. It performs the same function
 * as the corresponding declaration macro, and thus is redundant.
 */
#define QN_FUSION_DEFINE_FUNCTIONS_numeric(TYPE, ... /* PREFIX */)              \
__VA_ARGS__ bool lexical_numeric_check(const TYPE *);


/**
 * This macro generates both an enum definition and lexical serialization
 * functions for that enum.
 * 
 * Example usage:
 * \code
 * QN_DEFINE_LEXICAL_ENUM(Token, (IdToken, "ID")(ClassToken, "CLASS")(TokenCount, NULL))
 * \endcode
 * 
 * \param ENUM                          Name of the enumeration.
 * \param ELEMENTS                      Sequence of enumeration's elements with
 *                                      their string representation.
 * \param PREFIX                        Optional function definition prefix,
 *                                      e.g. <tt>inline</tt>.
 */
#define QN_DEFINE_LEXICAL_ENUM(ENUM, ELEMENTS, ... /* PREFIX */)                \
enum ENUM {                                                                     \
    BOOST_PP_VARIADIC_SEQ_FOR_EACH(QN_DEFINE_LEXICAL_ENUM_STEP_I, ~, ELEMENTS)  \
};                                                                              \
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(ENUM, ELEMENTS, ##__VA_ARGS__)

#define QN_DEFINE_LEXICAL_ENUM_STEP_I(R, DATA, ELEMENT)                         \
    BOOST_PP_TUPLE_ELEM(0, ELEMENT),


/**
 * \see QN_DEFINE_LEXICAL_ENUM
 */
#define QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(ENUM, ELEMENTS, ... /* PREFIX */)  \
    QN_DEFINE_EXPLICIT_ENUM_LEXICAL_SERIALIZER_FACTORY_FUNCTION(ENUM, ELEMENTS, ##__VA_ARGS__) \
    QN_DEFINE_FACTORY_ENUM_LEXICAL_FUNCTIONS(ENUM, ##__VA_ARGS__)


/**
 * This macro generates lexical serialization functions for the given enum
 * that has an associated metaobject (e.g. an enum inside a QObject or a 
 * class declared with Q_GADGET).
 *
 * Example usage:
 * \code
 * QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qt, Orientations)
 * \endcode
 *
 * \param SCOPE                         Enumeration's scope.
 * \param ENUM                          Name of the enumeration.
 */
#define QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(SCOPE, ENUM, ... /* PREFIX */) \
    QN_DEFINE_METAOBJECT_ENUM_LEXICAL_SERIALIZATION_FACTORY_FUNCTION(SCOPE, ENUM, ##__VA_ARGS__) \
    QN_DEFINE_FACTORY_ENUM_LEXICAL_FUNCTIONS(SCOPE::ENUM, ##__VA_ARGS__)


/**
 * \internal
 */
#define QN_DEFINE_EXPLICIT_ENUM_LEXICAL_SERIALIZER_FACTORY_FUNCTION(ENUM, ELEMENTS, ... /* PREFIX */) \
__VA_ARGS__ QnEnumLexicalSerializerData create_enum_lexical_serializer_data(const ENUM *) { \
    QnEnumLexicalSerializerData data;                                           \
    BOOST_PP_VARIADIC_SEQ_FOR_EACH(QN_DEFINE_EXPLICIT_ENUM_LEXICAL_SERIALIZER_FACTORY_FUNCTION_STEP_I, ~, ELEMENTS) \
    data.setNumeric(QnLexical::is_numerically_serializable<ENUM>::value);       \
    data.setFlagged(QnSerialization::is_flags<ENUM>::value);                    \
    return std::move(data);                                                     \
}

#define QN_DEFINE_EXPLICIT_ENUM_LEXICAL_SERIALIZER_FACTORY_FUNCTION_STEP_I(R, DATA, ELEMENT) \
    if(!QnLexicalDetail::isNullString(BOOST_PP_TUPLE_ELEM(1, ELEMENT)))         \
        data.insert ELEMENT;


/**
 * \internal
 */
#define QN_DEFINE_METAOBJECT_ENUM_LEXICAL_SERIALIZATION_FACTORY_FUNCTION(SCOPE, ENUM, ... /* PREFIX */)   \
__VA_ARGS__ QnEnumLexicalSerializerData create_enum_lexical_serializer_data(const SCOPE::ENUM *) { \
    QnEnumLexicalSerializerData data;                                           \
    data.load(&SCOPE::staticMetaObject, BOOST_PP_STRINGIZE(ENUM));              \
    data.setNumeric(QnLexical::is_numerically_serializable<SCOPE::ENUM>::value); \
    data.setFlagged(QnSerialization::is_flags<SCOPE::ENUM>::value);             \
    return std::move(data);                                                     \
}


/**
 * \internal
 */
#define QN_DEFINE_FACTORY_ENUM_LEXICAL_FUNCTIONS(TYPE, ... /* PREFIX */)  \
    QN_DEFINE_FACTORY_ENUM_LEXICAL_FUNCTIONS_I(TYPE, BOOST_PP_CAT(qn_enumLexicalSerializerData_instance, __LINE__), ##__VA_ARGS__)

#define QN_DEFINE_FACTORY_ENUM_LEXICAL_FUNCTIONS_I(TYPE, STATIC_NAME, ... /* PREFIX */) \
template<class T> void this_macro_cannot_be_used_in_header_files();             \
template<> void this_macro_cannot_be_used_in_header_files<TYPE>() {};           \
                                                                                \
Q_GLOBAL_STATIC_WITH_ARGS(QnEnumLexicalSerializerData, STATIC_NAME, (create_enum_lexical_serializer_data(static_cast<const TYPE *>(NULL)))) \
                                                                                \
__VA_ARGS__ void serialize(const TYPE &value, QString *target) {                \
    STATIC_NAME()->serialize(value, target);                                    \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(const QString &value, TYPE *target) {              \
    return STATIC_NAME()->deserialize(value, target);                           \
}


namespace Qt {
    /**
     * Workaround for accessing Qt namespace metaobject.
     */
    extern const QMetaObject &staticMetaObject;
}


#endif // QN_LEXICAL_ENUM_H
