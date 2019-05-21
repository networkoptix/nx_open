#include <algorithm>
#include <cstring>

#include <nx/kit/test.h>
#include <nx/kit/utils.h>

namespace nx {
namespace kit {
namespace utils {
namespace test {

TEST(utils, getProcessCmdLineArgs)
{
    const auto& args = getProcessCmdLineArgs();

    ASSERT_FALSE(args.empty());
    ASSERT_TRUE(args[0].find("nx_kit_ut") != std::string::npos);

    std::cerr << "Process has " << args.size() << " arg(s):" << std::endl;
    for (const auto& arg: args)
        std::cerr << nx::kit::utils::toString(arg) << std::endl;
}

TEST(utils, baseName)
{
    ASSERT_STREQ("", baseName(""));
    ASSERT_STREQ("", baseName("/"));
    ASSERT_STREQ("bar", baseName("/foo/bar"));
    ASSERT_STREQ("bar", baseName("foo/bar"));

    #if defined(_WIN32)
        ASSERT_STREQ("bar", baseName("d:bar"));
        ASSERT_STREQ("bar", baseName("X:\\bar"));
    #endif
}

TEST(utils, getProcessName)
{
    const std::string processName = getProcessName();

    ASSERT_FALSE(processName.empty());
    ASSERT_TRUE(processName.find(".exe") == std::string::npos);
    ASSERT_TRUE(processName.find('/') == std::string::npos);
    ASSERT_TRUE(processName.find('\\') == std::string::npos);
}

TEST(utils, alignUp)
{
    ASSERT_EQ(0U, alignUp(0, 0));
    ASSERT_EQ(17U, alignUp(17, 0));
    ASSERT_EQ(48U, alignUp(48, 16));
    ASSERT_EQ(48U, alignUp(42, 16));
    ASSERT_EQ(7U, alignUp(7, 7));
    ASSERT_EQ(8U, alignUp(7, 8));
}

TEST(utils, misalignedPtr)
{
    uint8_t data[1024];
    const auto aligned = (uint8_t*) alignUp((intptr_t) data, 32);
    ASSERT_EQ(0, (intptr_t) aligned % 32);
    uint8_t* const misaligned = misalignedPtr(data);
    ASSERT_TRUE((intptr_t) misaligned % 32 != 0);
}

TEST(utils, toString_string)
{
    ASSERT_STREQ("\"abc\"", toString(std::string("abc")));
}

TEST(utils, toString_ptr)
{
    ASSERT_STREQ("null", toString((const void*) nullptr));
    ASSERT_STREQ("null", toString(nullptr));

    const int dummyInt = 42;
    char expectedS[100];
    snprintf(expectedS, sizeof(expectedS), "%p", &dummyInt);
    ASSERT_EQ(expectedS, toString(&dummyInt));
}

TEST(utils, toString_bool)
{
    ASSERT_STREQ("true", toString(true));
    ASSERT_STREQ("false", toString(false));
}

TEST(utils, toString_number)
{
    ASSERT_STREQ("42", toString(42));
    ASSERT_STREQ("3.14", toString(3.14));
}

TEST(utils, toString_uint8_t)
{
    ASSERT_STREQ("42", toString((uint8_t) 42));
}

TEST(utils, toString_char)
{
    ASSERT_STREQ("'C'", toString('C'));
    ASSERT_STREQ(R"('\'')", toString('\''));
    ASSERT_STREQ(R"('\xFF')", toString('\xFF'));
}

TEST(utils, toString_char_ptr)
{
    ASSERT_STREQ("\"str\"", toString("str"));
    ASSERT_STREQ("\"str\\\"with_quote\"", toString("str\"with_quote"));
    ASSERT_STREQ(R"("str\\with_backslash")", toString("str\\with_backslash"));
    ASSERT_STREQ(R"("str\twith_tab")", toString("str\twith_tab"));
    ASSERT_STREQ(R"("str\nwith_newline")", toString("str\nwith_newline"));
    ASSERT_STREQ(R"("str\x7Fwith_127")", toString("str\x7Fwith_127"));
    ASSERT_STREQ(R"("str\x1Fwith_31")", toString("str\x1Fwith_31"));
    ASSERT_STREQ(R"("str\xFFwith_255")", toString("str\xFFwith_255"));
}

} // namespace test
} // namespace utils
} // namespace kit
} // namespace nx
