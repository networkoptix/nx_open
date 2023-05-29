// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

/**@file
 * Wrapper for the original Json11 header. Puts Json11 classes into namespace nx::kit and makes
 * them exported from nx_kit dynamic lib.
 *
 * This file should be included into projects using Json11 via nx_kit, instead of Json11 original
 * header json11.hpp. Then, use nx::kit::Json - the main class of the Json11 lib.
 */

// Includes from json11.hpp are duplicated here to put them in the global namespace.
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <initializer_list>

//-------------------------------------------------------------------------------------------------
// Make the Json11 classes exported from nx_kit dynamic lib.

namespace nx {
namespace kit {
namespace detail { //< Prohibits to refer to Json11 classes as nx::kit::json11::*.
namespace json11 { //< Such namespace is used in the original Json11 source.

class NX_KIT_API Json;
class NX_KIT_API JsonValue; //< Considered private in Json11, but is exported for debugging.

} // namespace json11
} // namespace detail
} // namespace kit
} // namespace nx

//-------------------------------------------------------------------------------------------------
// Explicitly instantiate and export a type used in class Json - suppresses an MSVC warning.

#if defined(_MSC_VER)
    namespace std {

    template class NX_KIT_API std::shared_ptr<nx::kit::detail::json11::JsonValue>;

    } // namespace std
#endif

//-------------------------------------------------------------------------------------------------
// Include the original Json11 header and make its classes available in namespace nx::kit.

namespace nx {
namespace kit {

namespace detail {

#include "../../json11/json11.hpp" //< Original Json11 header.

} // namespace detail

using namespace detail::json11; //< Allow to refer to the Json11 class as nx::kit::Json.

} // namespace kit
} // namespace nx

//-------------------------------------------------------------------------------------------------
// Additional features.

namespace nx {
namespace kit {

NX_KIT_API std::string jsonTypeToString(Json::Type type);

} // namespace kit
} // namespace nx
