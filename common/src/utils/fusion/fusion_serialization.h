#ifndef QN_FUSION_SERIALIZATION_H
#define QN_FUSION_SERIALIZATION_H

#ifndef Q_MOC_RUN
#include <boost/preprocessor/tuple/enum.hpp>
#endif 

#include "fusion.h"
#include "fusion_detail.h"

namespace QnFusion {
    /**
     * \param D                         Data type.
     * \returns                         Serialization visitor type that was
     *                                  registered for the given data type.
     * \see QN_FUSION_REGISTER_VISITORS
     */
    template<class D>
    struct serialization_visitor_type {};

    /**
     * \param D                         Data type.
     * \returns                         Deserialization visitor type that was
     *                                  registered for the given data type.
     * \see QN_FUSION_REGISTER_VISITORS
     */
    template<class D>
    struct deserialization_visitor_type {};
    
    template<class D>
    struct has_serialization_visitor_type:
        QnFusionDetail::has_type<serialization_visitor_type<D> >
    {};

    template<class D>
    struct has_deserialization_visitor_type:
        QnFusionDetail::has_type<deserialization_visitor_type<D> >
    {};



    template<class T, class D>
    void serialize(const T &value, D *target) {
        static_assert(is_adapted<T>::value, "Type T must have a fusion adaptor defined.");
        static_assert(has_serialization_visitor_type<D>::value, "Type D must have a fusion serialization visitor defined.");

        typename serialization_visitor_type<D>::type visitor(*target);
        QnFusion::visit_members(value, visitor);
    }

    template<class T, class D>
    bool deserialize(const D &value, T *target) {
        static_assert(is_adapted<T>::value, "Type T must have a fusion adaptor defined.");
        static_assert(has_deserialization_visitor_type<D>::value, "Type D must have a fusion deserialization visitor defined.");

        typename deserialization_visitor_type<D>::type visitor(value);
        return QnFusion::visit_members(*target, visitor);
    }

    template<class T, class D, class Context>
    void serialize(Context *ctx, const T &value, D *target) {
        static_assert(is_adapted<T>::value, "Type T must have a fusion adaptor defined.");
        static_assert(has_serialization_visitor_type<D>::value, "Type D must have a fusion serialization visitor defined.");

        typename serialization_visitor_type<D>::type visitor(ctx, *target);
        QnFusion::visit_members(value, visitor);
    }

    template<class T, class D, class Context>
    bool deserialize(Context *ctx, const D &value, T *target) {
        static_assert(is_adapted<T>::value, "Type T must have a fusion adaptor defined.");
        static_assert(has_deserialization_visitor_type<D>::value, "Type D must have a fusion deserialization visitor defined.");

        typename deserialization_visitor_type<D>::type visitor(ctx, value);
        return QnFusion::visit_members(*target, visitor);
    }

} // namespace QnFusion

#ifndef Q_MOC_RUN



// TODO: get rid of these. There is no real profit here.

/**
 * This macro registers fusion visitors that are to be used when serializing
 * and deserializing to/from the given data class. This macro must be used 
 * in global namespace.
 * 
 * Defining the visitors makes all fusion adapted classes instantly (de)serializable
 * to/from the given data class.
 * 
 * \param DATA_CLASS                    Data class to define visitors for.
 * \param SERIALIZATION_VISITOR         Serialization visitor class.
 * \param DESERIALIZATION_VISITOR       Deserialization visitor class.
 */
#define QN_FUSION_REGISTER_SERIALIZATION_VISITORS(DATA_CLASS, SERIALIZATION_VISITOR, DESERIALIZATION_VISITOR) \
    QN_FUSION_REGISTER_SERIALIZATION_VISITOR(DATA_CLASS, SERIALIZATION_VISITOR) \
    QN_FUSION_REGISTER_DESERIALIZATION_VISITOR(DATA_CLASS, DESERIALIZATION_VISITOR)

#define QN_FUSION_REGISTER_SERIALIZATION_VISITORS_TPL(TPL_DEF, DATA_CLASS, SERIALIZATION_VISITOR, DESERIALIZATION_VISITOR) \
    QN_FUSION_REGISTER_SERIALIZATION_VISITOR(TPL_DEF, DATA_CLASS, SERIALIZATION_VISITOR) \
    QN_FUSION_REGISTER_DESERIALIZATION_VISITOR(TPL_DEF, DATA_CLASS, DESERIALIZATION_VISITOR)


/**
 * \see QN_FUSION_REGISTER_VISITORS
 */
#define QN_FUSION_REGISTER_SERIALIZATION_VISITOR(DATA_CLASS, SERIALIZATION_VISITOR) \
    QN_FUSION_REGISTER_SERIALIZATION_VISITOR_TPL((), (DATA_CLASS), (SERIALIZATION_VISITOR))

#define QN_FUSION_REGISTER_SERIALIZATION_VISITOR_TPL(TPL_DEF, DATA_CLASS, SERIALIZATION_VISITOR) \
namespace QnFusion {                                                            \
    template<BOOST_PP_TUPLE_ENUM(TPL_DEF)>                                      \
    struct serialization_visitor_type<BOOST_PP_TUPLE_ENUM(DATA_CLASS)> {        \
        typedef BOOST_PP_TUPLE_ENUM(SERIALIZATION_VISITOR) type;                \
    };                                                                          \
}


/**
 * \see QN_FUSION_REGISTER_VISITORS
 */
#define QN_FUSION_REGISTER_DESERIALIZATION_VISITOR(DATA_CLASS, DESERIALIZATION_VISITOR) \
    QN_FUSION_REGISTER_DESERIALIZATION_VISITOR_TPL((), (DATA_CLASS), (DESERIALIZATION_VISITOR))

#define QN_FUSION_REGISTER_DESERIALIZATION_VISITOR_TPL(TPL_DEF, DATA_CLASS, DESERIALIZATION_VISITOR) \
namespace QnFusion {                                                            \
    template<BOOST_PP_TUPLE_ENUM(TPL_DEF)>                                      \
    struct deserialization_visitor_type<BOOST_PP_TUPLE_ENUM(DATA_CLASS)> {      \
        typedef BOOST_PP_TUPLE_ENUM(DESERIALIZATION_VISITOR) type;              \
    };                                                                          \
}

#else // Q_MOC_RUN

/* Qt moc chokes on these. We make its life easier. */
#define QN_FUSION_REGISTER_SERIALIZATION_VISITORS(...)
#define QN_FUSION_REGISTER_SERIALIZATION_VISITORS_TPL(...)
#define QN_FUSION_REGISTER_SERIALIZATION_VISITOR(...)
#define QN_FUSION_REGISTER_SERIALIZATION_VISITOR_TPL(...)
#define QN_FUSION_REGISTER_DESERIALIZATION_VISITOR(...)
#define QN_FUSION_REGISTER_DESERIALIZATION_VISITOR_TPL(...)

#endif // Q_MOC_RUN


#endif // QN_FUSION_SERIALIZATION_H
