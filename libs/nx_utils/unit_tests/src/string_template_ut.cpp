#include <gtest/gtest.h>

#include <nx/utils/string_template.h>

namespace nx::utils::test {

void testUpperTemplate(const QString& template_, const QString& expectedResult)
{
    ASSERT_EQ(
        expectedResult,
        stringTemplate(template_, [](auto n) { return n.toString().toUpper(); }));
}

TEST(stringTemplate, main)
{
    testUpperTemplate("Just text", "Just text");
    testUpperTemplate("Some value is {value}", "Some value is VALUE");
    testUpperTemplate("v1={v1}, v2={v2}", "v1=V1, v2=V2");
}

} // namespace nx::utils::test
