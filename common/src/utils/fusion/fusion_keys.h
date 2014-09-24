#ifndef QN_FUSION_KEYS_H
#define QN_FUSION_KEYS_H

#include <boost/preprocessor/cat.hpp>

/**
 * This macro defines a new fusion key. It must be used in global namespace.
 * Defined key can then be accessed from the QnFusion namespace.
 * 
 * \param KEY                           Key to define.
 */
#define QN_FUSION_DEFINE_KEY(KEY)                                               \
namespace QnFusion {                                                            \
    struct BOOST_PP_CAT(KEY, _type) {};                                         \
    namespace {                                                                 \
        const BOOST_PP_CAT(KEY, _type) KEY = {};                                \
    }                                                                           \
}

/**
 * \param KEY                           Fusion key.
 * \returns                             C++ type of the provided fusion key.
 */
#define QN_FUSION_KEY_TYPE(KEY)                                                 \
    QnFusion::BOOST_PP_CAT(KEY, _type)

QN_FUSION_DEFINE_KEY(base)
QN_FUSION_DEFINE_KEY(member_index)
QN_FUSION_DEFINE_KEY(member_count)
QN_FUSION_DEFINE_KEY(object_declval)
QN_FUSION_DEFINE_KEY(getter)
QN_FUSION_DEFINE_KEY(setter)
QN_FUSION_DEFINE_KEY(setter_tag)
QN_FUSION_DEFINE_KEY(checker)
QN_FUSION_DEFINE_KEY(name)
QN_FUSION_DEFINE_KEY(c_name)
QN_FUSION_DEFINE_KEY(sql_placeholder_name)
QN_FUSION_DEFINE_KEY(classname)
QN_FUSION_DEFINE_KEY(c_classname)
QN_FUSION_DEFINE_KEY(optional)

#define QN_FUSION_PROPERTY_IS_EXTENDED_FOR_base true
#define QN_FUSION_PROPERTY_EXTENSION_FOR_base(KEY, VALUE) (base, std::declval<VALUE>())

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_name true
#define QN_FUSION_PROPERTY_TYPE_FOR_name QString
#define QN_FUSION_PROPERTY_IS_EXTENDED_FOR_name true
#define QN_FUSION_PROPERTY_EXTENSION_FOR_name(KEY, VALUE) (name, QStringLiteral(VALUE))(c_name, VALUE)(sql_placeholder_name, QN_FUSION_LIT_CAT(":", VALUE))

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_c_name true
#define QN_FUSION_PROPERTY_TYPE_FOR_c_name const char *

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_sql_placeholder_name true
#define QN_FUSION_PROPERTY_TYPE_FOR_sql_placeholder_name QString

#define QN_FUSION_PROPERTY_IS_EXTENDED_FOR_setter true
#define QN_FUSION_PROPERTY_EXTENSION_FOR_setter(KEY, VALUE) (setter, VALUE)(setter_tag, QnFusionDetail::make_access_setter_category(access_type())) 

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_classname true
#define QN_FUSION_PROPERTY_TYPE_FOR_classname QString
#define QN_FUSION_PROPERTY_IS_EXTENDED_FOR_classname true
#define QN_FUSION_PROPERTY_EXTENSION_FOR_classname(KEY, VALUE) (classname, QStringLiteral(VALUE))(c_classname, VALUE)

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_c_classname true
#define QN_FUSION_PROPERTY_TYPE_FOR_c_classname const char *

#define QN_FUSION_PROPERTY_IS_TYPED_FOR_optional true
#define QN_FUSION_PROPERTY_TYPE_FOR_optional bool



/**
 * \internal
 * 
 * Produces a <tt>QStringLiteral</tt> from the given character literals.
 * Works around a MSVC bug that prevents us to simply use <tt>QStringLiteral("a" "b")</tt>.
 */
#define QN_FUSION_LIT_CAT(A, B)                                                 \
    QN_FUSION_LIT_CAT_I(A, B)

#ifdef _MSC_VER
#   define QN_FUSION_LIT_CAT_I(A, B) QStringLiteral(A BOOST_PP_CAT(L, B))
#else
#   define QN_FUSION_LIT_CAT_I(A, B) QStringLiteral(A B)
#endif


#endif // QN_FUSION_KEYS_H
