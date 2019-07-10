#include <vector>

#include <gtest/gtest.h>

#include <nx/utils/argument_parser.h>

namespace nx::utils::test {

class ArgumentParser:
    public ::testing::Test
{
protected:
    void whenParseArgs(std::vector<const char*> args)
    {
        m_argumentParser.parse((int) args.size(), args.data());
    }

    void thenNamedArgsAreFound(const std::map<QString, QString>& args)
    {
        for (const auto& [name, value]: args)
        {
            const auto arg = m_argumentParser.get(name);
            ASSERT_TRUE(arg);
            ASSERT_EQ(*arg, value);
        }
    }

    void andTrailingArgsPresent(const std::vector<QString>& expected)
    {
        ASSERT_EQ(expected, m_argumentParser.getUnnamedArgs());
    }

private:
    utils::ArgumentParser m_argumentParser;
};

TEST_F(ArgumentParser, parsing_args)
{
    whenParseArgs(
        {"-o", "file", "unnamed0", "-m", "mode", "--long=value", "unnamed1", "unnamed2"});

    thenNamedArgsAreFound({{"o", "file"}, {"m", "mode"}, {"long", "value"}});
    andTrailingArgsPresent({"unnamed0", "unnamed1", "unnamed2"});
}

} // namespace nx::utils::test
