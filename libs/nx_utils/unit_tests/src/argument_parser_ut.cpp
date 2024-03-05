// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

    void andPositionalArgsPresent(const std::vector<QString>& expected)
    {
        ASSERT_EQ(expected, m_argumentParser.getPositionalArgs());
    }

private:
    nx::ArgumentParser m_argumentParser;
};

TEST_F(ArgumentParser, parsing_args)
{
    whenParseArgs(
        {"-o", "file", "positional0", "-m", "mode", "--long=value", "positional1", "positional2"});

    thenNamedArgsAreFound({{"o", "file"}, {"m", "mode"}, {"long", "value"}});
    andPositionalArgsPresent({"positional0", "positional1", "positional2"});
}

} // namespace nx::utils::test
