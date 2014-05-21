#include <utils/common/model_functions.h>

#include "ptz_tour.h"
#include "ptz_preset.h"
#include "ptz_limits.h"
#include "ptz_data.h"
#include "ptz_object.h"

/*namespace Qn {
    QN_FUSION_DECLARE_FUNCTIONS_numeric(Qn::PtzDataFields)
}*/

//QN_FUSION_DECLARE_FUNCTIONS_numeric(Qn::PtzDataFields)

/*
bool lexical_numeric_check(const Qn::PtzDataFields *);

using namespace QnTypeTraits;

bool lexical_numeric_check(const na *);*/

namespace DDD {
    using namespace QnTypeTraits;

    template<class T>
    yes_type lexical_numeric_check_test(const T &, const decltype(lexical_numeric_check(std::declval<const T *>())) * = NULL);
    no_type lexical_numeric_check_test(...);

    template<class T>
    struct is_numerically_serializable:
        std::integral_constant<bool, sizeof(lexical_numeric_check_test(std::declval<T>())) == sizeof(yes_type)>
    {};
}



static_assert(QnLexical::is_numerically_serializable<Qn::PtzDataFields>::value, "!!!");

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Qt, Orientations)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (QnPtzPreset)(QnPtzTourSpot)(QnPtzTour)(QnPtzLimits)(QnPtzObject)(QnPtzData),
    (json)(eq),
    _Fields
)

