#include "test_descriptor.h"

#include <nx/fusion/model_functions.h>

namespace nx::analytics {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    TestDescriptor,
    (json),
    nx_analytics_TestDescriptor_Fields,
    (brief, true))

} // namespace nx::analytics
