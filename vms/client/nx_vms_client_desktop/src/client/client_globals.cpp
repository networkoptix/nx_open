#include "client_globals.h"
#include <nx/fusion/serialization/lexical_enum.h>

/**
 * Those roles are used in the BlueBox integration and shall not be changed without an aggreement.
 */
static_assert(Qn::ItemGeometryRole == 288);
static_assert(Qn::ItemImageDewarpingRole == 295);
static_assert(Qn::ItemRotationRole == 297);
static_assert(Qn::ItemFlipRole == 299);

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, ThumbnailStatus,
(Qn::ThumbnailStatus::Invalid, "Invalid")
(Qn::ThumbnailStatus::Loading, "Loading")
(Qn::ThumbnailStatus::Loaded, "Loaded")
(Qn::ThumbnailStatus::NoData, "NoData")
(Qn::ThumbnailStatus::Refreshing, "Refreshing")
)
