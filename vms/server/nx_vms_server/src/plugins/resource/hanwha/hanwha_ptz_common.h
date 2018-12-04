#pragma once

#include <map>

#include <QtCore/QString>

#include <core/ptz/ptz_constants.h>
#include <nx/core/ptz/type.h>

namespace nx {
namespace vms::server {
namespace plugins {

using HanwhaPtzCapabilitiesMap = std::map<nx::core::ptz::Type, Ptz::Capabilities>;
using HanwhaPtzParameterName = QString;

using HanwhaPtzRangeByParameter = std::map<HanwhaPtzParameterName, HanwhaRange>;
using HanwhaPtzRangeMap = std::map<nx::core::ptz::Type, HanwhaPtzRangeByParameter>;

} // namespace ptz
} // namespace core
} // namespace nx
