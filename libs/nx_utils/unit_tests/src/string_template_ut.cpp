#include <gtest/gtest.h>

#include <nx/utils/string_template.h>

namespace nx::utils::test {

void testUpperTemplate(const QString& template_, const QString& expectedResult)
{
    static const auto resolve = [](const auto& n) { return n.toUpper() + "_24h"; };
    ASSERT_EQ(expectedResult, stringTemplate(template_, "$", resolve));
}

TEST(StringTemplate, main)
{
    testUpperTemplate("Just text", "Just text");
    testUpperTemplate("Some value is $value", "Some value is VALUE_24h");
    testUpperTemplate("v1=$v1, v2=$v2", "v1=V1_24h, v2=V2_24h");
    testUpperTemplate("$sPrefix$sValue is ok", "SPREFIX_24hSVALUE_24h is ok");
}

} // namespace nx::utils::test
