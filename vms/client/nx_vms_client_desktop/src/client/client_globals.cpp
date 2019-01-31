#include "client_globals.h"
#include <nx/fusion/serialization/lexical_enum.h>

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(Qn, ThumbnailStatus,
(Qn::ThumbnailStatus::Invalid, "Invalid")
(Qn::ThumbnailStatus::Loading, "Loading")
(Qn::ThumbnailStatus::Loaded, "Loaded")
(Qn::ThumbnailStatus::NoData, "NoData")
(Qn::ThumbnailStatus::Refreshing, "Refreshing")
)
