// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/utils/log/log.h>
#include <nx/utils/exception.h>

namespace nx::utils::test {

using namespace std;

struct NewError: public nx::utils::Exception
{
    virtual QString message() const { return QString(kErrorMessage); }
    static constexpr const char* kErrorMessage = "test_error";
};

TEST(Exceptions, Base)
{
    try
    {
        throw NewError();
    }
    catch (const nx::utils::Exception& e)
    {
        EXPECT_STREQ(e.what(), NewError::kErrorMessage);
    }
}

TEST(Exceptions, Wrapping)
{
    EXPECT_EQ(7, NX_WRAP_EXCEPTION(3 + 4, runtime_error, logic_error, ""));
    EXPECT_EQ(nx::unwrapNestedErrors(runtime_error("I am an error!")), "I am an error!");
    EXPECT_EQ(nx::toString(runtime_error("I am an error!")), "I am an error!");
    EXPECT_EQ(nx::format("%1").args(runtime_error("I am an error!")).toQString(), "I am an error!");

    try
    {
        NX_WRAP_EXCEPTION(throw runtime_error("I am an error!"), runtime_error, logic_error, "");
    }
    catch (const logic_error& e)
    {
        EXPECT_EQ(nx::unwrapNestedErrors(e), "I am an error!");
        EXPECT_EQ(nx::toString(e), "I am an error!");
        EXPECT_EQ(nx::format("%1").args(e).toQString(), "I am an error!");
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
        EXPECT_EQ(nx::unwrapNestedErrors(e),
            "Ultra high level error: Very high level error: Higher level error: I am an error!");
        EXPECT_EQ(nx::toString(e),
            "Ultra high level error: Very high level error: Higher level error: I am an error!");
        EXPECT_EQ(nx::format("%1").args(e).toQString(),
            "Ultra high level error: Very high level error: Higher level error: I am an error!");
    }
}

} // namespace nx::utils::test
