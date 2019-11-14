#include <gtest/gtest.h>

#include <nx/utils/exceptions.h>

namespace nx::utils::test {

using namespace std;

TEST(Exceptions, Wrapping)
{
    EXPECT_EQ(7, NX_WRAP_EXCEPTION(3 + 4, runtime_error, logic_error, ""));
    EXPECT_EQ(unwrapNestedErrors(runtime_error("I am an error!")), "I am an error!");
    EXPECT_EQ(toString(runtime_error("I am an error!")), "I am an error!");

    try
    {
        NX_WRAP_EXCEPTION(throw runtime_error("I am an error!"), runtime_error, logic_error, "");
    }
    catch (const logic_error& e)
    {
        EXPECT_EQ(unwrapNestedErrors(e), "I am an error!");
        EXPECT_EQ(toString(e), "I am an error!");
    }

    try
    {
        NX_WRAP_EXCEPTION(NX_WRAP_EXCEPTION(NX_WRAP_EXCEPTION(throw runtime_error("I am an error!"),
            runtime_error, logic_error, "Higher level error"),
            logic_error, runtime_error, "Very high level error"),
            runtime_error, logic_error, "Ultra high level error");
    }
    catch (const logic_error& e)
    {
        EXPECT_EQ(unwrapNestedErrors(e),
            "Ultra high level error: Very high level error: Higher level error: I am an error!");
        EXPECT_EQ(toString(e),
            "Ultra high level error: Very high level error: Higher level error: I am an error!");
    }
}

} // namespace nx::utils::test
