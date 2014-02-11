#ifndef QN_ENUM_NAME_MAPPER_H
#define QN_ENUM_NAME_MAPPER_H

#include <cassert>

#ifndef Q_MOC_RUN
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/mpl/bool.hpp>
#endif

#include <QtCore/QMetaEnum>
#include <QtCore/QMetaObject>

#include <utils/common/warnings.h>
#include <utils/common/forward.h>

#include "adl_wrapper.h"


template<class Enum>
class QnTypedEnumNameMapper;


/**
 * <tt>QnEnumNameMapper</tt> supplies routines for fast <tt>QString</tt>-<tt>enum</tt> conversion.
 * 
 * It can be constructed from an enumeration registered with Qt metaobject system,
 * in which case the appropriate mapping is created automatically.
 */
class QnEnumNameMapper {
public:
    QnEnumNameMapper() {}

    QnEnumNameMapper(const QMetaObject *metaObject, const char *enumName);

    void insert(int value, const char *name);
    void insert(int value, const QString &name);

    int value(const QString &name, int defaultValue) const;
    int value(const QString &name, bool *ok) const;
    QString name(int value, const QString &defaultValue = QString()) const;

    // TODO: #Elric support Qt-style flags

    template<class Enum>
    static QnTypedEnumNameMapper<Enum> create() {
        return createEnumNameMapper(adlWrap<Enum *>(NULL));
    }

private:
    QHash<int, QString> m_nameByValue;
    QHash<QString, int> m_valueByName;
};


template<class Enum>
class QnTypedEnumNameMapper: public QnEnumNameMapper {
    typedef QnEnumNameMapper base_type;
public:
    QN_FORWARD_CONSTRUCTOR(QnTypedEnumNameMapper, QnEnumNameMapper, {})

    void insert(Enum value, const char *name) {
        base_type::insert(value, name);
    }

    void insert(Enum value, const QString &name) {
        base_type::insert(value, name);
    }

    Enum value(const QString &name, Enum defaultValue) const {
        return static_cast<Enum>(base_type::value(name, defaultValue));
    }

    Enum value(const QString &name, bool *ok) const {
        return static_cast<Enum>(base_type::value(name, ok));
    }

    QString name(Enum value, const QString &defaultValue = QString()) const {
        return base_type::name(value, defaultValue);
    }
};


namespace QnEnumDetail {
    inline bool isNullString(const char *s) { return s == NULL; }
    inline bool isNullString(const QString &s) { return s.isNull(); }
} // namespace QnEnumDetail


/**
 * This macro generates both an enum definition and a <tt>createEnumNameMapper</tt>
 * template function that returns <tt>QnEnumNameMapper</tt> for the defined enum.
 * 
 * Example usage:
 * <pre>
 * QN_DEFINE_NAME_MAPPED_ENUM(Token, ((IdToken, "ID"))((ClassToken, "CLASS"))((TokenCount, NULL)))
 * </pre>
 * 
 * \param ENUM                          Name of the enumeration.
 * \param ELEMENTS                      Sequence of enumeration's elements with
 *                                      their string representation.
 */
#define QN_DEFINE_NAME_MAPPED_ENUM(ENUM, ELEMENTS, ... /* PREFIX */)            \
enum ENUM {                                                                     \
    BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_NAME_MAPPED_ENUM_VALUE_I, ~, ELEMENTS)      \
};                                                                              \
QN_DEFINE_EXPLICIT_ENUM_NAME_MAPPING(ENUM, ELEMENTS, ##__VA_ARGS__)


#define QN_DEFINE_EXPLICIT_ENUM_NAME_MAPPING(ENUM, ELEMENTS, ... /* PREFIX */)  \
__VA_ARGS__ QnTypedEnumNameMapper<ENUM> createEnumNameMapper(ENUM *) {          \
    QnTypedEnumNameMapper<ENUM> result;                                         \
    BOOST_PP_SEQ_FOR_EACH(QN_DEFINE_EXPLICIT_ENUM_NAME_MAPPING_VALUE_I, ~, ELEMENTS) \
    return result;                                                              \
}


#define QN_DEFINE_NAME_MAPPED_ENUM_VALUE_I(R, DATA, ELEMENT)                    \
    BOOST_PP_TUPLE_ELEM(0, ELEMENT),

#define QN_DEFINE_EXPLICIT_ENUM_NAME_MAPPING_VALUE_I(R, DATA, ELEMENT)          \
    if(!QnEnumDetail::isNullString(BOOST_PP_TUPLE_ELEM(1, ELEMENT)))            \
        result.insert ELEMENT;


/**
 * This macro generates a <tt>createEnumNameMapper</tt> template function
 * that returns <tt>QnEnumNameMapper</tt> for a given enumeration registered
 * with the scope's metaobject.
 *
 * \param SCOPE                         Enumeration's scope.
 * \param ENUM                          Name of the enumeration.
 */
#define QN_DEFINE_METAOBJECT_ENUM_NAME_MAPPING(SCOPE, ENUM, ... /* PREFIX */)   \
__VA_ARGS__ QnTypedEnumNameMapper<SCOPE::ENUM> createEnumNameMapper(SCOPE::ENUM *) { \
    return QnTypedEnumNameMapper<SCOPE::ENUM>(&SCOPE::staticMetaObject, BOOST_PP_STRINGIZE(ENUM)); \
}


#endif // QN_ENUM_NAME_MAPPER_H
