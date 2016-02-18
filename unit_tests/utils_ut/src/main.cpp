/**********************************************************
* Dec 29, 2015
* akolesnikov
***********************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>


int main(int argc, char **argv)
{
    ::testing::InitGoogleMock(&argc, argv);
    const int result = RUN_ALL_TESTS();
    return result;
}
