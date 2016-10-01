#pragma once

/** Creates gmock checker that verifies argument type with \a dynamic_cast. */
#define GMOCK_DYNAMIC_TYPE_MATCHER(T) ::testing::WhenDynamicCastTo<T>(::testing::An<T>())
