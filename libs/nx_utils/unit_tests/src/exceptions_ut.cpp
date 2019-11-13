#include <gtest/gtest.h>

#include <nx/utils/exceptions.h>

namespace nx::utils::test {

using namespace std;

TEST(Exceptions, Wrapping)
{
    EXPECT_EQ(7, WRAP_EXCEPTION(3 + 4, runtime_error, logic_error, ""));
    EXPECT_EQ(unwrapNestedErrors(runtime_error("I am an error!")), "I am an error!");

    try
    {
        WRAP_EXCEPTION(throw runtime_error("I am an error!"), runtime_error, logic_error, "");
    }
    catch (const logic_error& e)
    {
        EXPECT_EQ(unwrapNestedErrors(e), "I am an error!");
    }

    try
    {
        WRAP_EXCEPTION(throw runtime_error("I am an error!"), runtime_error, logic_error, "Zorz");
    }
    catch (const logic_error& e)
    {
        EXPECT_EQ(unwrapNestedErrors(e), "Zorz: I am an error!");
    }
}

} // namespace nx::utils::test
