#include <gtest/gtest.h>

#include <vector>

#include <nx/utils/placeholder_binder.h>

using namespace nx::utils;

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
    {"{:hello}, {:world}", {{"hello1", "Good bye"}, {"world", "James"}}, "{:hello}, James"},
    {"{:hello}, {:world}", {{"hello1", "Good bye"}, {"world1", "James"}}, "{:hello}, {:world}"},
    {"{:hello}, {:world}", {{"hello", "Good bye"}, {"world1", "James"}}, "Good bye, {:world}"},
};

TEST(PlaceholderBinder, bindPlaceholders)
{
    for (const auto& testCase: kTestCases)
    {
        PlaceholderBinder binder(testCase.stringTemplate);
        binder.bind(testCase.bindings);
        ASSERT_EQ(binder.str(), testCase.expectedResult);
    }
}
