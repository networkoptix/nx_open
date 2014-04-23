#ifndef QN_FUSION_SERIALIZATION_H
#define QN_FUSION_SERIALIZATION_H

#ifndef Q_MOC_RUN
#include <boost/mpl/has_xxx.hpp>
#endif 

#include "fusion.h"

namespace QnFusionDetail {
    BOOST_MPL_HAS_XXX_TRAIT_DEF(type)
}

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
    typename boost::enable_if<boost::mpl::and_<QnFusion::has_visit_members<T>, QnFusion::has_serialization_visitor_type<D> >, void>::type
    serialize(const T &value, D *target) {
        typename QnFusion::serialization_visitor_type<D>::type visitor(*target);
        QnFusion::visit_members(value, visitor);
    }

    template<class T, class D>
    typename boost::enable_if<boost::mpl::and_<QnFusion::has_visit_members<T>, QnFusion::has_deserialization_visitor_type<D> >, bool>::type
    deserialize(const D &value, T *target) {
        typename QnFusion::deserialization_visitor_type<D>::type visitor(value);
        return QnFusion::visit_members(*target, visitor);
    }

    template<class T, class D, class Context>
    typename boost::enable_if<boost::mpl::and_<QnFusion::has_visit_members<T>, QnFusion::has_serialization_visitor_type<D> >, void>::type
    serialize(Context *ctx, const T &value, D *target) {
        typename QnFusion::serialization_visitor_type<D>::type visitor(ctx, *target);
        QnFusion::visit_members(value, visitor);
    }

    template<class T, class D, class Context>
    typename boost::enable_if<boost::mpl::and_<QnFusion::has_visit_members<T>, QnFusion::has_deserialization_visitor_type<D> >, bool>::type
    deserialize(Context *ctx, const D &value, T *target) {
        typename QnFusion::deserialization_visitor_type<D>::type visitor(ctx, value);
        return QnFusion::visit_members(*target, visitor);
    }

} // namespace QnFusion


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

/**
 * \see QN_FUSION_REGISTER_VISITORS
 */
#define QN_FUSION_REGISTER_SERIALIZATION_VISITOR(DATA_CLASS, SERIALIZATION_VISITOR) \
namespace QnFusion {                                                            \
    template<>                                                                  \
    struct serialization_visitor_type<DATA_CLASS>:                              \
        boost::mpl::identity<SERIALIZATION_VISITOR>                             \
    {};                                                                         \
}

/**
 * \see QN_FUSION_REGISTER_VISITORS
 */
#define QN_FUSION_REGISTER_DESERIALIZATION_VISITOR(DATA_CLASS, DESERIALIZATION_VISITOR) \
namespace QnFusion {                                                            \
    template<>                                                                  \
    struct deserialization_visitor_type<DATA_CLASS>:                            \
        boost::mpl::identity<DESERIALIZATION_VISITOR>                           \
    {};                                                                         \
}


#endif // QN_FUSION_SERIALIZATION_H
