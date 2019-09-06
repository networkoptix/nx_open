#include <gtest/gtest.h>

#include <nx/vms/utils/metrics/rule_monitors.h>
#include <nx/vms/utils/metrics/value_monitors.h>

namespace nx::vms::utils::metrics::test {

class MetricsRules: public ::testing::Test
{
public:
    MetricsRules():
        getA([this](int) { return a; }),
        getB([this](int) { return b; })
    {
        monitors["a"] = std::make_unique<RuntimeValueMonitor<int>>(r, getA);
        monitors["b"] = std::make_unique<RuntimeValueMonitor<int>>(r, getB);
    }

protected:
    int r = 0;
    api::metrics::Value a, b;
    Getter<int> getA, getB;
    ValueMonitors monitors;
};

TEST_F(MetricsRules, ValueGenerator)
{
    ASSERT_THROW(parseFormulaOrThrow("str hello", monitors), std::domain_error);
    ASSERT_THROW(parseFormulaOrThrow("const", monitors), std::domain_error);
    ASSERT_THROW(parseFormulaOrThrow("= %a", monitors), std::domain_error);
    ASSERT_THROW(parseFormulaOrThrow("= %a %x", monitors), std::domain_error);

    const auto helloWorld = parseFormulaOrThrow("const HelloWorld", monitors);
    EXPECT_EQ(helloWorld(), api::metrics::Value("HelloWorld"));

    const auto number7 = parseFormulaOrThrow("const 7", monitors);
    EXPECT_EQ(number7(), api::metrics::Value(7));

    const auto plusAB = parseFormulaOrThrow("+ %a %b", monitors);
    const auto minusAB = parseFormulaOrThrow("- %a %b", monitors);
    const auto equalAB = parseFormulaOrThrow("= %a %b", monitors);
    const auto notEqualAB = parseFormulaOrThrow("!= %a %b", monitors);
    const auto greaterAB = parseFormulaOrThrow("> %a %b", monitors);

    a = b = api::metrics::Value();
    EXPECT_EQ(plusAB(), api::metrics::Value());
    EXPECT_EQ(minusAB(), api::metrics::Value());
    EXPECT_EQ(equalAB(), api::metrics::Value());
    EXPECT_EQ(notEqualAB(), api::metrics::Value());
    EXPECT_EQ(greaterAB(), api::metrics::Value());

    a = 7;
    EXPECT_EQ(plusAB(), api::metrics::Value());
    EXPECT_EQ(minusAB(), api::metrics::Value());
    EXPECT_EQ(equalAB(), api::metrics::Value());
    EXPECT_EQ(notEqualAB(), api::metrics::Value());
    EXPECT_EQ(greaterAB(), api::metrics::Value());

    b = 8;
    EXPECT_EQ(plusAB(), api::metrics::Value(15));
    EXPECT_EQ(minusAB(), api::metrics::Value(-1));
    EXPECT_EQ(equalAB(), api::metrics::Value(false));
    EXPECT_EQ(notEqualAB(), api::metrics::Value(true));
    EXPECT_EQ(greaterAB(), api::metrics::Value(false));

    a = 7;
    EXPECT_EQ(parseFormulaOrThrow("greaterOrEqual %a 7", monitors)(), api::metrics::Value(true));
    EXPECT_EQ(parseFormulaOrThrow("greaterOrEqual %a 10", monitors)(), api::metrics::Value(false));

    a = "hello";
    EXPECT_EQ(parseFormulaOrThrow("equal %a hello", monitors)(), api::metrics::Value(true));
    EXPECT_EQ(parseFormulaOrThrow("notEqual %a hello", monitors)(), api::metrics::Value(false));
}

} // namespace nx::vms::utils::metrics::test
