// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdexcept>
#include <tuple>

#include <nx/utils/type_utils.h>

/** Creates gmock checker that verifies argument type with dynamic_cast. */
#define GMOCK_DYNAMIC_TYPE_MATCHER(T) ::testing::WhenDynamicCastTo<T>(::testing::An<T>())

/**
 * Analogue of gtest's ASSERT_EQ but supports placing in non-void methods (throws on failure).
 */
#define NX_GTEST_ASSERT_EQ(expected, actual) do \
{ \
    bool resultE31cf27915b242999a4e22c7757d94de = false; \
    [&]() -> void \
    { \
        ASSERT_EQ(expected, actual); \
        resultE31cf27915b242999a4e22c7757d94de = true; \
    }(); \
    if (!resultE31cf27915b242999a4e22c7757d94de) \
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

#define NX_GTEST_ASSERT_GT(actual, expected) do \
{ \
    NX_GTEST_ASSERT_TRUE(actual > expected); \
} while (0)

#define NX_GTEST_ASSERT_LT(actual, expected) do \
{ \
    NX_GTEST_ASSERT_TRUE(actual < expected); \
} while (0)

#define NX_GTEST_ASSERT_ANY_OF(expectedValues, actual) do \
{ \
    const auto& actualValue = actual; \
    bool foundMatchingValue = false; \
    nx::utils::tuple_for_each( \
        std::make_tuple expectedValues, \
        [&actualValue, &foundMatchingValue](const auto& value) mutable \
            { foundMatchingValue |= actualValue == value; }); \
    ASSERT_TRUE(foundMatchingValue); \
} while (0)

#define NX_GTEST_ASSERT_NO_THROW(expression) do \
{ \
    try \
    { \
        expression; \
    } \
    catch (std::exception& e) \
    { \
        ASSERT_TRUE(false) << e.what(); \
    } \
} while (0)

#define NX_GTEST_WRAP(expr) \
[&]() \
{ \
    expr; \
}()

#define NX_GTEST_ASSERT_NE(expected, actual) \
    NX_GTEST_WRAP(ASSERT_NE(expected, actual))
