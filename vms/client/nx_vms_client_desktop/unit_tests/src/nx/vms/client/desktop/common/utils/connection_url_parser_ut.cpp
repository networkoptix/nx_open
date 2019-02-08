#include <gtest/gtest.h>

#include <nx/vms/client/desktop/common/utils/connection_url_parser.h>

namespace nx::vms::client::desktop {
namespace test {

using InputAndOutput = std::pair<QString, std::string>;

class ConnectionUrlParserTest: public ::testing::TestWithParam<InputAndOutput>
{
};

TEST_P(ConnectionUrlParserTest, assertParsedCorrectly)
{
    auto [input, expectedOutput] = GetParam();
    const auto url = parseConnectionUrlFromUserInput(input);
    ASSERT_EQ(url.toString().toStdString(), expectedOutput);
}

std::vector<InputAndOutput> testData = {
    {"localhost",                                   "https://localhost:7001"},
    {"https://localhost/test",                      "https://localhost:7001"},
    {"https://localhost?param=1",                   "https://localhost:7001"},
    {"https://localhost#anchor",                    "https://localhost:7001"},
    {"https://user@localhost",                      "https://localhost:7001"},
    {"https://user:password@localhost",             "https://localhost:7001"},
    {"http://user:password@localhost:7002?p1=test&p2=&p3=a#anchor", "http://localhost:7002"},
    {"http://127.0.0.1",                            "http://127.0.0.1:7001"},
    {"htt://127.0.0.1",                             "https://127.0.0.1:7001"},
    {"fe80::a4b8:7e4e:7ebe:8277",                   "https://[fe80::a4b8:7e4e:7ebe:8277]:7001"},
    {"[fe80::a4b8:7e4e:7ebe:8277]",                 "https://[fe80::a4b8:7e4e:7ebe:8277]:7001"},
    {"[fe80::a4b8:7e4e:7ebe:8277]:7002",            "https://[fe80::a4b8:7e4e:7ebe:8277]:7002"},
    {"https://[fe80::a4b8:7e4e:7ebe:8277]:7002",    "https://[fe80::a4b8:7e4e:7ebe:8277]:7002"},
    {"fe80::a4b8:7e4e:7ebe:8277%11",                "https://[fe80::a4b8:7e4e:7ebe:8277%11]:7001"},
    {"[fe80::a4b8:7e4e:7ebe:8277%11]",              "https://[fe80::a4b8:7e4e:7ebe:8277%11]:7001"},
    {"[fe80::a4b8:7e4e:7ebe:8277%11]:7002",         "https://[fe80::a4b8:7e4e:7ebe:8277%11]:7002"},
    {"https://[fe80::a4b8:7e4e:7ebe:8277%11]:7002", "https://[fe80::a4b8:7e4e:7ebe:8277%11]:7002"}
};

INSTANTIATE_TEST_CASE_P(UrlFromUserInput,
    ConnectionUrlParserTest,
    ::testing::ValuesIn(testData));


} // namespace test
} // namespace nx::vms::client::desktop
