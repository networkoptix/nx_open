#include <gtest/gtest.h>

#include <nx/system_commands/detail/escape_quotes.h>

namespace nx::system_commands::test {

TEST(EscapeQuotes, EscapeQuotes)
{
    ASSERT_EQ(R"('\''url)", escapeSingleQuotes(R"('url)"));
    ASSERT_EQ(R"(user'\'' nam '\''e '\'')", escapeSingleQuotes(R"(user' nam 'e ')"));
    ASSERT_EQ(R"('\''`id > /tmp/id.txt`'\'')", escapeSingleQuotes(R"('`id > /tmp/id.txt`')"));
}

} // namespace nx::system_commands::test
