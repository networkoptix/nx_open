#include <gtest/gtest.h>

#include <nx/vms/utils/metrics/rule_monitors.h>
#include <nx/vms/utils/metrics/value_monitors.h>

#include "test_providers.h"

namespace nx::vms::utils::metrics::test {

class MetricsRules: public ::testing::Test
{
public:
    MetricsRules(): resource(0)
    {
        for (const auto& id: {"a", "b"})
        {
            monitors[id] = std::make_unique<ValueHistoryMonitor<TestResource>>(
                resource,
                m_getters[id] = [id](const auto& r) { return r.current(id); },
                m_watches[id] = [id](const auto& r, auto change) { return r.monitor(id, std::move(change)); });
        }
    }

protected:
    TestResource resource;
    ValueMonitors monitors;

private:
    std::map<QString, Getter<TestResource>> m_getters;
    std::map<QString, Watch<TestResource>> m_watches;
};

class MetricsValueGenerator: public MetricsRules {};

TEST_F(MetricsValueGenerator, Errors)
{
    for (const auto& [formula, expectedError]: std::map<QString, QString>{
        {"str hello", "Unsupported function: str"},
        {"const", "Missing parameter in formula: const"},
        {"= %a", "Missing parameter in formula: = %a"},
        {"= %a %x", "Unknown value id: x"},
    })
    {
        try
        {
            parseFormulaOrThrow(formula, monitors);
            FAIL() << "Did not throw: "  << formula.toStdString();
        }
        catch(const std::domain_error& error)
        {
            EXPECT_EQ(QString(error.what()), QString(expectedError)) << formula.toStdString();
        }
    }
}

TEST_F(MetricsValueGenerator, Binary)
{
    const auto helloWorld = parseFormulaOrThrow("const HelloWorld", monitors);
    EXPECT_EQ(helloWorld(), api::metrics::Value("HelloWorld"));

    const auto number7 = parseFormulaOrThrow("const 7", monitors);
    EXPECT_EQ(number7(), api::metrics::Value(7));

    const auto plusAB = parseFormulaOrThrow("+ %a %b", monitors);
    const auto minusAB = parseFormulaOrThrow("- %a %b", monitors);
    const auto equalAB = parseFormulaOrThrow("= %a %b", monitors);
    const auto notEqualAB = parseFormulaOrThrow("!= %a %b", monitors);
    const auto greaterAB = parseFormulaOrThrow("> %a %b", monitors);

    EXPECT_EQ(plusAB(), api::metrics::Value());
    EXPECT_EQ(minusAB(), api::metrics::Value());
    EXPECT_EQ(equalAB(), api::metrics::Value());
    EXPECT_EQ(notEqualAB(), api::metrics::Value());
    EXPECT_EQ(greaterAB(), api::metrics::Value());

    resource.update("a", 7);
    EXPECT_EQ(plusAB(), api::metrics::Value());
    EXPECT_EQ(minusAB(), api::metrics::Value());
    EXPECT_EQ(equalAB(), api::metrics::Value());
    EXPECT_EQ(notEqualAB(), api::metrics::Value());
    EXPECT_EQ(greaterAB(), api::metrics::Value());

    resource.update("b", 8);
    EXPECT_EQ(plusAB(), api::metrics::Value(15));
    EXPECT_EQ(minusAB(), api::metrics::Value(-1));
    EXPECT_EQ(equalAB(), api::metrics::Value(false));
    EXPECT_EQ(notEqualAB(), api::metrics::Value(true));
    EXPECT_EQ(greaterAB(), api::metrics::Value(false));

    resource.update("a", 7);
    EXPECT_EQ(parseFormulaOrThrow("greaterOrEqual %a 7", monitors)(), api::metrics::Value(true));
    EXPECT_EQ(parseFormulaOrThrow("greaterOrEqual %a 10", monitors)(), api::metrics::Value(false));

    resource.update("a", "hello");
    EXPECT_EQ(parseFormulaOrThrow("equal %a hello", monitors)(), api::metrics::Value(true));
    EXPECT_EQ(parseFormulaOrThrow("notEqual %a hello", monitors)(), api::metrics::Value(false));
}

TEST_F(MetricsValueGenerator, Duration)
{
    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);

    const auto count = parseFormulaOrThrow("count %a 30m", monitors);
    const auto count7 = parseFormulaOrThrow("countValues %a 40m 7", monitors);
    const auto countHello = parseFormulaOrThrow("countValues %a 50m Hello", monitors);

    EXPECT_EQ(count(), api::metrics::Value(1));
    EXPECT_EQ(count7(), api::metrics::Value(0));
    EXPECT_EQ(countHello(), api::metrics::Value(0));

    timeShift.applyAbsoluteShift(std::chrono::minutes(5));
    resource.update("a", 7);
    EXPECT_EQ(count(), api::metrics::Value(2));
    EXPECT_EQ(count7(), api::metrics::Value(1));
    EXPECT_EQ(countHello(), api::metrics::Value(0));

    timeShift.applyAbsoluteShift(std::chrono::minutes(15));
    resource.update("a", "Hello");
    EXPECT_EQ(count(), api::metrics::Value(3));
    EXPECT_EQ(count7(), api::metrics::Value(1));
    EXPECT_EQ(countHello(), api::metrics::Value(1));

    timeShift.applyAbsoluteShift(std::chrono::minutes(25));
    resource.update("a", "hello");
    EXPECT_EQ(count(), api::metrics::Value(4));
    EXPECT_EQ(count7(), api::metrics::Value(1));
    EXPECT_EQ(countHello(), api::metrics::Value(1));

    timeShift.applyAbsoluteShift(std::chrono::minutes(50));
    resource.update("a", "hello");
    EXPECT_EQ(count(), api::metrics::Value(2));
    EXPECT_EQ(count7(), api::metrics::Value(1));
    EXPECT_EQ(countHello(), api::metrics::Value(1));
    // TODO: Check period expirations.
}

} // namespace nx::vms::utils::metrics::test
