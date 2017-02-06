/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <nx/utils/argument_parser.h>


TEST(ArgumentParser, common)
{
    const char* argv[] = {"/path/to/bin", "-a", "aaa", "--arg1=arg1", "--arg2", "-b", "-c", "ccc", "-ddd", "val_ddd", "-e", "eee", "--arg3=" };
    const auto argc = sizeof(argv) / sizeof(*argv);

    nx::utils::ArgumentParser args(argc, argv);

    ASSERT_TRUE((bool)args.get("a"));
    ASSERT_EQ(*args.get("a"), "aaa");

    ASSERT_TRUE((bool)args.get("arg1"));
    ASSERT_EQ(*args.get("arg1"), "arg1");

    ASSERT_TRUE((bool)args.get("arg2"));
    ASSERT_TRUE(args.get("arg2")->isEmpty());

    ASSERT_TRUE((bool)args.get("b"));
    ASSERT_TRUE(args.get("b")->isEmpty());

    ASSERT_TRUE((bool)args.get("arg3"));
    ASSERT_TRUE(args.get("arg3")->isEmpty());

    ASSERT_TRUE((bool)args.get("c"));
    ASSERT_EQ(*args.get("c"), "ccc");

    ASSERT_TRUE((bool)args.get("e"));
    ASSERT_EQ(*args.get("e"), "eee");

    ASSERT_TRUE((bool)args.get("ddd"));
    ASSERT_EQ(*args.get("ddd"), "val_ddd");
}
