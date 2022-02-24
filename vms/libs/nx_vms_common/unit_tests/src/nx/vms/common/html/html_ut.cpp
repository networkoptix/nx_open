// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/common/html/html.h>

namespace nx::vms::common::html {
namespace test {

TEST(Html, tagged)
{
    ASSERT_EQ(tagged("source", "html"), "<html>source</html>");
}

} // namespace test
} // namespace nx::vms::common::html
