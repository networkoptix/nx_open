#include <gtest/gtest.h>

#include <nx/system_commands/detail/escape_quotes.h>

namespace nx::system_commands::test {

TEST(EscapeQuotes, EscapeQuotes)
{
    ASSERT_EQ(R"('\''url)", escapeQuotes(R"('url)"));
    ASSERT_EQ(R"(user'\'' nam '\''e '\'')", escapeQuotes(R"(user' nam 'e ')"));
    ASSERT_EQ(R"('\''`id > /tmp/id.txt`'\'')", escapeQuotes(R"('`id > /tmp/id.txt`')"));
}

} // namespace nx::system_commands::test
