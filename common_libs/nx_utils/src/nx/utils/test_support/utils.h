#pragma once

#ifdef _WIN32   //< Temporary solution until gmock on linux is updated.
/** Creates gmock checker that verifies argument type with \a dynamic_cast. */
#define GMOCK_DYNAMIC_TYPE_MATCHER(T) ::testing::WhenDynamicCastTo<T>(::testing::An<T>())
#define GMOCK_DYNAMIC_TYPE_MATCHER_PRESENT
#else
#define GMOCK_DYNAMIC_TYPE_MATCHER(T) ::testing::_
#endif
