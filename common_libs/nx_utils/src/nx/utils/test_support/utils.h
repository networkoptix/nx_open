#pragma once

#include <stdexcept>

/** Creates gmock checker that verifies argument type with \a dynamic_cast. */
#define GMOCK_DYNAMIC_TYPE_MATCHER(T) ::testing::WhenDynamicCastTo<T>(::testing::An<T>())

/**
 * Analogue of gtest's ASSERT_EQ but supports placing in non-void methods (throws on failure).
 */
#define NX_GTEST_ASSERT_EQ(expected, actual) do \
{ \
    bool result = false; \
    [&]() -> void \
    { \
        ASSERT_EQ(expected, actual); \
        result = true; \
    }(); \
    if (!result) \
    { \
        throw std::runtime_error( \
            "This is work around ASSERT_* inability to be used " \
            "in non-void method. Just ignore..."); \
    } \
} while (0)

#define NX_GTEST_ASSERT_TRUE(expected) do \
{ \
    NX_GTEST_ASSERT_EQ(true, expected); \
} while (0)

#define NX_GTEST_ASSERT_FALSE(expected) do \
{ \
    NX_GTEST_ASSERT_EQ(false, expected); \
} while (0)
