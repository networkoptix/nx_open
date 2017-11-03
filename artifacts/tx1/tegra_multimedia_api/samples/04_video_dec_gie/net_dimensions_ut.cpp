#include <memory>
#include <sstream>

#include <nx/kit/test.h>

#include "net_dimensions.h"

static void test(const std::string& text, NetDimensions expectedDimensions)
{
    std::istringstream input(text);
    const NetDimensions dimensions = getNetDimensions(input, "test", {0, 0});
    ASSERT_EQ(expectedDimensions.width, dimensions.width);
    ASSERT_EQ(expectedDimensions.height, dimensions.height);
}

static void testInvalid(const std::string& text)
{
    std::istringstream input(text);
    const NetDimensions dimensions = getNetDimensions(input, "testInvalid", {0, 0});
    ASSERT_EQ(0, dimensions.width);
    ASSERT_EQ(0, dimensions.height);
}

TEST(net_dimensions, carnetSample)
{
    test(R"prototxt(
        input_dim: 1
        input_dim: 3
        input_dim: 540
        input_dim: 960
    )prototxt", {960, 540});
}

TEST(net_dimensions, pednetSample)
{
    test(R"prototxt(
        input: "data"
        input_shape {
          dim: 1
          dim: 3
          dim: 512
          dim: 1024
        }
    )prototxt", {1024, 512});
}

TEST(net_dimensions, invalidMissingWidth)
{
    testInvalid(R"prototxt(
        input: "data"
        input_shape {
          dim: 1
          dim: 3
          dim: 512
        }
    )prototxt");
}

TEST(net_dimensions, invalidMissingBothWidthAndHeight)
{
    testInvalid(R"prototxt(
        input: "data"
        input_shape {
          dim: 1
          dim: 3
        }
    )prototxt");
}

TEST(net_dimensions, invalidMissingAnyDimensionLines)
{
    testInvalid(R"prototxt(
        input: "data"
    )prototxt");
}

TEST(net_dimensions, invalidEmptyText)
{
    testInvalid("");
}

int main(int argc, char** argv)
{
    return nx::kit::test::runAllTests();
}
