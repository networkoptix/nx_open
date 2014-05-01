#ifndef QN_LEXICAL_ENUM_SERIALIZER_H
#define QN_LEXICAL_ENUM_SERIALIZER_H

#include "lexical.h"

#include <type_traits> /* For std::true_type, std::false_type. */
#include <utility> /* For std::move. */

#include <QtCore/QHash>
#include <QtCore/QString>

struct QMetaObject;

namespace QnLexicalDetail {
    template<class T>
    struct is_flags:
        std::false_type
    {};

    template<class Enum>
    struct is_flags<QFlags<Enum> >:
        std::true_type
    {};

    inline bool isNullString(const char *s) { return s == NULL; }
    inline bool isNullString(const QString &s) { return s.isNull(); }

} // namespace QnLexicalDetail


class QnLexicalEnumSerializerData {
public:
    QnLexicalEnumSerializerData() {}

    void insert(int value, const QString &name);
    void insert(const QMetaObject *metaObject, const char *enumName);

    void serializeEnum(int value, QString *target);
    bool deserializeEnum(const QString &value, int *target);

    void serializeFlags(int value, QString *target);
    bool deserializeFlags(const QString &value, int *target);

    template<class T>
    void serialize(const T &value, QString *target) const {
        (QnLexicalDetail::is_flags<T>() ? serializeFlags : serializeEnum)(value, target);
    }

    template<class T>
    bool deserialize(const QString &value, T *target) const {
        int tmp;
        if(!(QnLexicalDetail::is_flags<T>() ? deserializeFlags : deserializeEnum)(value, &tmp))
            return false;
        *target = static_cast<T>(tmp);
        return true;
    }

private:
    QHash<int, QString> m_nameByValue;
    QHash<QString, int> m_valueByName;
    QString m_enumName;
};


template<class Enum>
class QnLexicalEnumSerializer: public QnLexicalSerializer {
    static_assert(sizeof(Enum) <= sizeof(int), "Enumeration types larger than int in size are not supported.");
    typedef QnLexicalSerializer base_type;

public:
    QnLexicalEnumSerializer(): 
        base_type(qMetaTypeId<Enum>()) 
    {}

    QnLexicalEnumSerializer(const QnLexicalEnumSerializerData &data): 
        base_type(qMetaTypeId<Enum>()),
        m_data(data)
    {}

    const QnLexicalEnumSerializerData &data() const {
        return m_data;
    }

protected:
    virtual void serializeInternal(const void *value, QString *target) const override {
        m_data.deserialize(*static_cast<const Enum *>(value), target);
    }

    virtual bool deserializeInternal(const QString &value, void *target) const override {
        int tmp;
        if(!m_data.deserialize(value, &tmp))
            return false;
        *static_cast<Enum *>(target) = static_cast<Enum>(tmp);
    }

private:
    QnLexicalEnumSerializerData m_data;
};


/**
 * This macro generates both an enum definition and lexical serialization
 * functions for that enum.
 * 
 * Example usage:
 * \code
 * QN_DEFINE_LEXICAL_SERIALIZABLE_ENUM(Token, (IdToken, "ID")(ClassToken, "CLASS")(TokenCount, NULL))
 * \endcode
 * 
 * \param ENUM                          Name of the enumeration.
 * \param ELEMENTS                      Sequence of enumeration's elements with
 *                                      their string representation.
 */
#define QN_DEFINE_LEXICAL_ENUM(ENUM, ELEMENTS, ... /* PREFIX */)   \
enum ENUM {                                                                     \
    BOOST_PP_VARIADIC_SEQ_FOR_EACH(QN_DEFINE_LEXICAL_ENUM_STEP_I, ~, ELEMENTS) \
};                                                                              \
QN_DEFINE_EXPLICIT_LEXICAL_ENUM_FUNCTIONS(ENUM, ELEMENTS, ##__VA_ARGS__)

#define QN_DEFINE_LEXICAL_ENUM_STEP_I(R, DATA, ELEMENT)                         \
    BOOST_PP_TUPLE_ELEM(0, ELEMENT),


/**
 * \see QN_DEFINE_LEXICAL_ENUM
 */
#define QN_DEFINE_EXPLICIT_LEXICAL_ENUM_FUNCTIONS(ENUM, ELEMENTS, ... /* PREFIX */)  \
    QN_DEFINE_EXPLICIT_LEXICAL_ENUM_SERIALIZER_FACTORY_FUNCTION(ENUM, ELEMENTS, ##__VA_ARGS__) \
    QN_DEFINE_FACTORY_LEXICAL_ENUM_FUNCTIONS(ENUM, ##__VA_ARGS__)


/**
 * This macro generates lexical serialization functions for the given enum
 * that has an associated metaobject (e.g. an enum inside a QObject or a 
 * class declared with Q_GADGET).
 *
 * Example usage:
 * \code
 * QN_DEFINE_METAOBJECT_LEXICAL_ENUM_FUNCTIONS(Qt, Orientations)
 * \endcode
 *
 * \param SCOPE                         Enumeration's scope.
 * \param ENUM                          Name of the enumeration.
 */
#define QN_DEFINE_METAOBJECT_LEXICAL_ENUM_FUNCTIONS(SCOPE, ENUM, ... /* PREFIX */) \
    QN_DEFINE_METAOBJECT_LEXICAL_ENUM_SERIALIZATION_FACTORY_FUNCTION(SCOPE, ENUM, ##__VA_ARGS__) \
    QN_DEFINE_FACTORY_LEXICAL_ENUM_FUNCTIONS(SCOPE::ENUM, ##__VA_ARGS__)


/**
 * \internal
 */
#define QN_DEFINE_EXPLICIT_LEXICAL_ENUM_SERIALIZER_FACTORY_FUNCTION(ENUM, ELEMENTS, ... /* PREFIX */) \
__VA_ARGS__ QnLexicalEnumSerializer<ENUM> new_lexical_serializer(ENUM *) {      \
    QnLexicalEnumSerializerData data;                                           \
    BOOST_PP_VARIADIC_SEQ_FOR_EACH(QN_DEFINE_EXPLICIT_LEXICAL_ENUM_SERIALIZER_FACTORY_FUNCTION_STEP_I, ~, ELEMENTS) \
    return new QnLexicalEnumSerializer<ENUM>(std::move(data));                  \
}

#define QN_DEFINE_EXPLICIT_LEXICAL_ENUM_SERIALIZER_FACTORY_FUNCTION_STEP_I(R, DATA, ELEMENT) \
    if(!QnLexicalDetail::isNullString(BOOST_PP_TUPLE_ELEM(1, ELEMENT)))         \
        data.insert ELEMENT;


/**
 * \internal
 */
#define QN_DEFINE_METAOBJECT_LEXICAL_ENUM_SERIALIZATION_FACTORY_FUNCTION(SCOPE, ENUM, ... /* PREFIX */)   \
__VA_ARGS__ QnLexicalEnumSerializer<SCOPE::ENUM> new_lexical_serializer(SCOPE::ENUM *) { \
    QnLexicalEnumSerializerData data;                                           \
    data.insert(&SCOPE::staticMetaObject, BOOST_PP_STRINGIZE(ENUM));            \
    return new QnLexicalEnumSerializer<ENUM>(std::move(data));                  \
}


/**
 * \internal
 */
#define QN_DEFINE_FACTORY_LEXICAL_ENUM_FUNCTIONS(TYPE, ... /* PREFIX */)  \
    QN_DEFINE_FACTORY_LEXICAL_ENUM_FUNCTIONS_I(TYPE, BOOST_PP_CAT(qn_lexicalEnumSerializer_instance, __LINE__), ##__VA_ARGS__)

#define QN_DEFINE_FACTORY_LEXICAL_ENUM_FUNCTIONS_I(TYPE, STATIC_NAME, ... /* PREFIX */) \
template<class T> void this_macro_cannot_be_used_in_header_files();             \
template<> void this_macro_cannot_be_used_in_header_files<TYPE>() {};           \
                                                                                \
Q_GLOBAL_STATIC_WITH_ARGS(QnLexicalEnumSerializer<TYPE>, STATIC_NAME, (new_lexical_serializer(static_cast<TYPE *>(NULL)))) \
                                                                                \
__VA_ARGS__ void serialize(const TYPE &value, QString *target) {                \
    STATIC_NAME()->data().serialize(value, target);                             \
}                                                                               \
                                                                                \
__VA_ARGS__ bool deserialize(const QString &value, TYPE *target) {              \
    return STATIC_NAME()->data().deserialize(value, target);                    \
}



#endif // QN_LEXICAL_ENUM_SERIALIZER_H
