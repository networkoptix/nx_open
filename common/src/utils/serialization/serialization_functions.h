#ifndef QN_SERIALIZATION_FUNCTIONS_H
#define QN_SERIALIZATION_FUNCTIONS_H

#include <utils/preprocessor/variadic_seq_for_each.h>

#include <boost/preprocessor/seq/fold_left.hpp>

namespace Qss {
    /**
     * This class is the external interface that is to be used by visitor
     * classes.
     */
    template<class MemberAdaptor>
    struct AdaptorWrapper {
        template<class Tag>
        static decltype(MemberAdaptor::get(Tag())) get() {
            return MemberAdaptor::get(Tag());
        }
    };

} // namespace QssDetail


/**
 *
 */
#define QSS_DEFINE_CLASS_ADAPTOR(CLASS, MEMBER_SEQ, GLOBAL_SEQ)                 \
    QSS_DEFINE_CLASS_ADAPTOR_I(CLASS, QSS_ENUM_SEQ(MEMBER_SEQ), GLOBAL_SEQ)

#define QSS_DEFINE_CLASS_ADAPTOR_I(CLASS, MEMBER_SEQ, GLOBAL_SEQ)               \
template<class T>                                                               \
struct QssMemberAdaptor;                                                        \
                                                                                \
template<>                                                                      \
struct QssMemberAdaptor<CLASS> {                                                \
    BOOST_PP_SEQ_FOR_EACH(QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_I, GLOBAL_SEQ, MEMBER_SEQ) \
};                                                                              \
                                                                                \
template<class Visitor>                                                         \
void visit_members(const TYPE &value, Visitor &&visitor) {                      \
    typedef TYPE value_type;                                                    \
    BOOST_PP_SEQ_FOR_EACH(QSS_DEFINE_CLASS_ADAPTOR_FUNCTION_STEP_I, GLOBAL_SEQ, MEMBER_SEQ) \
}


#define QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_I(R, GLOBAL_SEQ, PROPERTY_TUPLE)   \
    QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_II(                                    \
        BOOST_PP_TUPLE_ELEM(0, PROPERTY_TUPLE),                                 \
        GLOBAL_SEQ BOOST_PP_TUPLE_ELEM(1, PROPERTY_TUPLE)                       \
    )

#define QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_II(INDEX, PROPERTY_SEQ)            \
    struct BOOST_PP_CAT(Member, INDEX) {                                        \
        BOOST_PP_VARIADIC_SEQ_FOR_EACH(QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_STEP_I, ~, PROPERTY_SEQ) \
    };

#define QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_STEP_I(R, DATA, PROPERTY_TUPLE)    \
    QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_STEP_II PROPERTY_TUPLE

#define QSS_DEFINE_CLASS_ADAPTOR_OBJECT_STEP_STEP_II(TAG, VALUE)                \
    static QSS_TAG_TYPE(TAG, VALUE) get(const Qss::TAG &) {                     \
        return VALUE;                                                           \
    }


#define QSS_DEFINE_CLASS_ADAPTOR_FUNCTION_STEP_I(R, GLOBAL_SEQ, MEMBER_SEQ)     \
    QSS_DEFINE_CLASS_ADAPTOR_FUNCTION_STEP_II(                                  \
        BOOST_PP_TUPLE_ELEM(0, PROPERTY_TUPLE),                                 \
        GLOBAL_SEQ BOOST_PP_TUPLE_ELEM(1, PROPERTY_TUPLE)                       \
    )

#define QSS_DEFINE_CLASS_ADAPTOR_FUNCTION_STEP_II(INDEX, PROPERTY_SEQ)          \
    if(!visitor(Qss::AdaptorWrapper<QssMemberAdaptor<value_type>::BOOST_PP_CAT(Member, INDEX)>())) \
        return;


/**
 * This macro defines a new Qss tag. It must be used in global namespace.
 * Defined tag can then be accessed from the Qss namespace.
 * 
 * \param TAG                           Tag to define.
 */
#define QSS_DEFINE_TAG(TAG)                                                     \
namespace Qss { struct TAG {}; }

QSS_DEFINE_TAG(getter)
QSS_DEFINE_TAG(setter)
QSS_DEFINE_TAG(name)

#define QSS_TAG_IS_TYPED_FOR_name ,
#define QSS_TAG_TYPE_FOR_name QString

/**
 * This macro returns a type expression that can be used in return type of a
 * tag getter function. By default it simply returns <tt>decltype(VALUE)</tt>,
 * but in some cases this is not a valid expression (e.g. when <tt>VALUE</tt>
 * is a lambda). For such cases this macro provides an extension mechanism
 * that makes it possible to specify the type for the tag explicitly at
 * tag definition time.
 *
 * \param TAG                           Qss tag.
 * \param VALUE                         Value specified by the user for this tag.
 * \returns                             Type expression that should be used for
 *                                      return type of tag getter function.
 */
#define QSS_TAG_TYPE(TAG, VALUE)                                                \
    BOOST_PP_OVERLOAD(QSS_TAG_TYPE_I_, ~ BOOST_PP_CAT(QSS_TAG_IS_TYPED_FOR_, TAG) ~)(TAG, VALUE)
    
#define QSS_TAG_TYPE_I_1(TAG, VALUE) decltype(VALUE)
#define QSS_TAG_TYPE_I_2(TAG, VALUE) BOOST_PP_CAT(QSS_TAG_TYPE_FOR_, TAG)

/**
 * This macro transforms the elements of the provided sequence into
 * <tt>(index, element)</tt> tuples.
 * 
 * Example input: 
 * \code
 * (a)(b)(c)(d)
 * \endcode
 * 
 * Example output:
 * \code
 * ((0, a))((1, b))((2, c))((3, d))
 * \endcode
 * 
 * \param SEQ                           Sequence to transform.
 * \returns                             Transformed sequence.
 */
#define QSS_ENUM_SEQ(SEQ)                                                       \
    BOOST_PP_TUPLE_ELEM(                                                        \
        1,                                                                      \
        BOOST_PP_SEQ_FOLD_LEFT(QSS_ENUM_SEQ_STEP_I, (0, BOOST_PP_SEQ_NIL), SEQ) \
    )

#define QSS_ENUM_SEQ_STEP_I(S, STATE, ELEM)                                     \
    (                                                                           \
        BOOST_PP_INC(BOOST_PP_TUPLE_ELEM(0, STATE)),                            \
        BOOST_PP_TUPLE_ELEM(1, STATE)((BOOST_PP_TUPLE_ELEM(0, STATE), ELEM))    \
    )


#endif // QN_SERIALIZATION_FUNCTIONS_H
