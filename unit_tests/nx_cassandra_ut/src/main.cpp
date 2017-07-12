#define USE_GMOCK

#include <cstring>
#include <gtest/gtest.h>
#include "options.h"

Options* options()
{
    static Options options;
    return &options;
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    for (int i = 0; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--host") == 0 && i != argc - 1)
            options()->host = argv[i + 1];
    }

    return RUN_ALL_TESTS();
}
