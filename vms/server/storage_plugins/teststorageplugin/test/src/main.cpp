#include <gtest/gtest.h>
#include <plugin_api.h>

int main(int argc, const char* argv[])
{
    ::testing::InitGoogleTest(&argc, (char**)argv);
    return RUN_ALL_TESTS();
}