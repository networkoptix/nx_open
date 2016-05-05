/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include <gtest/gtest.h>

#include <utils/common/command_line_parser.h>


TEST(parseCmdArgs, common)
{
    char* argv[] = {"/path/to/bin", "-a", "aaa", "--arg1=arg1", "--arg2", "-b", "-c", "ccc", "-ddd", "val_ddd", "-e", "eee", "--arg3=" };
    const auto argc = sizeof(argv) / sizeof(*argv);

    std::multimap<QString, QString> args;
    parseCmdArgs(
        argc,
        argv,
        &args);
    ASSERT_EQ(args.size(), 8);

    ASSERT_TRUE(args.find("a") != args.end());
    ASSERT_EQ(args.find("a")->second, "aaa");
    ASSERT_TRUE(args.find("arg1") != args.end());
    ASSERT_EQ(args.find("arg1")->second, "arg1");
    ASSERT_TRUE(args.find("arg2") != args.end());
    ASSERT_TRUE(args.find("arg2")->second.isEmpty());
    ASSERT_TRUE(args.find("b") != args.end());
    ASSERT_TRUE(args.find("b")->second.isEmpty());
    ASSERT_TRUE(args.find("arg3") != args.end());
    ASSERT_TRUE(args.find("arg3")->second.isEmpty());
    ASSERT_TRUE(args.find("c") != args.end());
    ASSERT_EQ(args.find("c")->second, "ccc");
    ASSERT_TRUE(args.find("e") != args.end());
    ASSERT_EQ(args.find("e")->second, "eee");
    ASSERT_TRUE(args.find("ddd") != args.end());
    ASSERT_EQ(args.find("ddd")->second, "val_ddd");
}
