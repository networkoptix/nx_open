#include <gtest/gtest.h>

#include <vector>

#include <nx/vms/server/sdk_support/placeholder_binder.h>

using namespace nx::vms::server;

struct TestCase
{
    QString stringTemplate;
    std::map<QString, QString> bindings;
    QString expectedResult;
};

const std::vector<TestCase> kTestCases = {
    {"Hello, {:world}!", {{"world", "world"}}, "Hello, world!"},
    {"Hello, {:world}!", {{"world", "James"}}, "Hello, James!"},
    {"{:hello}, {:world}!", {{"hello", "Hello"}, {"world", "world"}}, "Hello, world!"},
    {"{:hello}, {:world}!", {{"hello", "Hello"}, {"world", "James"}}, "Hello, James!"},
    {"{:hello}, {:world}!", {{"hello", "Good bye"}, {"world", "James"}}, "Good bye, James!"},
    {"{:hello}, {:world}", {{"hello", "Good bye"}, {"world", "James"}}, "Good bye, James"},
    {"{:hello}, {:world}", {{"hello1", "Good bye"}, {"world", "James"}}, ", James"},
    {"{:hello}, {:world}", {{"hello1", "Good bye"}, {"world1", "James"}}, ", "},
    {"{:hello}, {:world}", {{"hello", "Good bye"}, {"world1", "James"}}, "Good bye, "},
};

TEST(PlaceholderBinder, bindPlaceholders)
{
    for (const auto& testCase: kTestCases)
    {
        sdk_support::PlaceholderBinder binder(testCase.stringTemplate);
        binder.bind(testCase.bindings);
        ASSERT_EQ(binder.str(), testCase.expectedResult);
    }
}
