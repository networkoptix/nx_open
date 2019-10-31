#include <gtest/gtest.h>

#include <nx/vms/utils/metrics/rule_monitors.h>
#include <nx/vms/utils/metrics/value_monitors.h>

#include "test_providers.h"

namespace nx::vms::utils::metrics::test {

class MetricsGenerators: public ::testing::Test
{
public:
    MetricsGenerators(): resource(0)
    {
        for (const auto& id: {"a", "b"})
        {
            monitors[id] = std::make_unique<ValueHistoryMonitor<TestResource>>(
                id,
                Scope::local,
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

TEST_F(MetricsGenerators, ValueErrors)
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
        catch(const std::logic_error& error)
        {
            EXPECT_EQ(QString(error.what()), QString(expectedError)) << formula.toStdString();
        }
    }
}

TEST_F(MetricsGenerators, ValueBinary)
{
    const auto helloWorld = parseFormulaOrThrow("const HelloWorld", monitors).generator;
    EXPECT_EQ(helloWorld(), api::metrics::Value("HelloWorld"));

    const auto number7 = parseFormulaOrThrow("const 7", monitors).generator;
    EXPECT_EQ(number7(), api::metrics::Value(7));

    const auto plusAB = parseFormulaOrThrow("add %a %b", monitors).generator;
    const auto minusAB = parseFormulaOrThrow("sub %a %b", monitors).generator;
    const auto equalAB = parseFormulaOrThrow("equal %a %b", monitors).generator;
    const auto notEqualAB = parseFormulaOrThrow("notEqual %a %b", monitors).generator;
    const auto greaterAB = parseFormulaOrThrow("greaterThan %a %b", monitors).generator;

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
    EXPECT_EQ(parseFormulaOrThrow("greaterOrEqual %a 7", monitors).generator(), api::metrics::Value(true));
    EXPECT_EQ(parseFormulaOrThrow("greaterOrEqual %a 10", monitors).generator(), api::metrics::Value(false));

    resource.update("a", "hello");
    EXPECT_EQ(parseFormulaOrThrow("equal %a hello", monitors).generator(), api::metrics::Value(true));
    EXPECT_EQ(parseFormulaOrThrow("notEqual %a hello", monitors).generator(), api::metrics::Value(false));
}

TEST_F(MetricsGenerators, ValueCounts)
{
    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);

    const auto count = parseFormulaOrThrow("count %a 30m", monitors).generator;
    const auto count7 = parseFormulaOrThrow("countValues %a 40m 7", monitors).generator;
    const auto countHello = parseFormulaOrThrow("countValues %a 50m Hello", monitors).generator;

    EXPECT_EQ(count(), api::metrics::Value(0));
    EXPECT_EQ(count7(), api::metrics::Value(0));
    EXPECT_EQ(countHello(), api::metrics::Value(0));

    timeShift.applyAbsoluteShift(std::chrono::minutes(5));
    resource.update("a", 7);

    EXPECT_EQ(count(), api::metrics::Value(1));
    EXPECT_EQ(count7(), api::metrics::Value(1));
    EXPECT_EQ(countHello(), api::metrics::Value(0));

    timeShift.applyAbsoluteShift(std::chrono::minutes(15));
    resource.update("a", "Hello");

    EXPECT_EQ(count(), api::metrics::Value(2));
    EXPECT_EQ(count7(), api::metrics::Value(1));
    EXPECT_EQ(countHello(), api::metrics::Value(1));

    timeShift.applyAbsoluteShift(std::chrono::minutes(25));
    resource.update("a", "hello");

    EXPECT_EQ(count(), api::metrics::Value(3));
    EXPECT_EQ(count7(), api::metrics::Value(1));
    EXPECT_EQ(countHello(), api::metrics::Value(1));

    timeShift.applyAbsoluteShift(std::chrono::minutes(50));
    resource.update("a", "hello");

    EXPECT_EQ(count(), api::metrics::Value(1));
    EXPECT_EQ(count7(), api::metrics::Value(0));
    EXPECT_EQ(countHello(), api::metrics::Value(1));
}

#define EXPECT_VALUE(GETTER, VALUE) \
{ \
    const auto value = GETTER(); \
    const auto number = value.toDouble(); \
    EXPECT_TRUE((number > ((VALUE) - 0.1)) && (number < ((VALUE) + 0.1))) \
        << "Value: " << value.toVariant().toString().toStdString() << std::endl; \
}

TEST_F(MetricsGenerators, ValueFloat)
{
    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);
    const auto sum = parseFormulaOrThrow("sum %b 1m", monitors).generator;
    const auto sampleAvg = parseFormulaOrThrow("sampleAvg %b 1m", monitors).generator;
    const auto deltaAvg = parseFormulaOrThrow("deltaAvg %b 30s", monitors).generator;
    const auto counterToAvg = parseFormulaOrThrow("counterToAvg %b 20s", monitors).generator;

    resource.update("b", 0);
    EXPECT_EQ(sum(), 0);
    EXPECT_EQ(sampleAvg(), api::metrics::Value());
    EXPECT_EQ(deltaAvg(), api::metrics::Value());
    EXPECT_EQ(counterToAvg(), api::metrics::Value());

    timeShift.applyAbsoluteShift(std::chrono::seconds(10));
    resource.update("b", 100);
    EXPECT_VALUE(sum, 100);
    EXPECT_VALUE(sampleAvg, 0);
    EXPECT_VALUE(deltaAvg, 10);
    EXPECT_EQ(counterToAvg(), 10);

    timeShift.applyAbsoluteShift(std::chrono::seconds(20));
    resource.update("b", 200);
    EXPECT_VALUE(sum, 300);
    EXPECT_VALUE(sampleAvg, 50);
    EXPECT_VALUE(deltaAvg, 15);
    EXPECT_EQ(counterToAvg(), 10);

    timeShift.applyAbsoluteShift(std::chrono::seconds(40));
    resource.update("b", 600);
    EXPECT_VALUE(sum, 900);
    EXPECT_VALUE(sampleAvg, 125);
    EXPECT_VALUE(deltaAvg, 26.6);
    EXPECT_EQ(counterToAvg(), 20);

    timeShift.applyAbsoluteShift(std::chrono::minutes(1));
    resource.update("b", 30);
    EXPECT_VALUE(sum, 930);
    EXPECT_VALUE(sampleAvg, 283.3);
    EXPECT_VALUE(deltaAvg, 21);
    EXPECT_EQ(counterToAvg(), -28.5);

    timeShift.applyAbsoluteShift(std::chrono::minutes(1) + std::chrono::seconds(30));
    EXPECT_VALUE(sum, 630);
    EXPECT_VALUE(sampleAvg, 248.3);
    EXPECT_VALUE(deltaAvg, 0); //< Out of calculation range.
    EXPECT_EQ(counterToAvg(), 0); //< Out of calculation range.
}

TEST_F(MetricsGenerators, ValueSpike)
{
    nx::utils::test::ScopedTimeShift timeShift(nx::utils::test::ClockType::steady);
    const auto sum = parseFormulaOrThrow("sum %b 10s", monitors).generator;
    const auto sampleAvg = parseFormulaOrThrow("sampleAvg %b 10s", monitors).generator;
    const auto deltaAvg = parseFormulaOrThrow("deltaAvg %b 10s", monitors).generator;

    resource.update("b", 0);
    EXPECT_EQ(sum(), api::metrics::Value(0));
    EXPECT_EQ(sampleAvg(), api::metrics::Value());
    EXPECT_EQ(deltaAvg(), api::metrics::Value());

    for (int i = 1; i <= 4; ++i)
    {
        timeShift.applyAbsoluteShift(std::chrono::seconds(i));
        resource.update("b", 0);
        EXPECT_EQ(sum(), 0);
        EXPECT_EQ(sampleAvg(), 0);
        EXPECT_EQ(deltaAvg(), 0);
    }

    timeShift.applyAbsoluteShift(std::chrono::seconds(4));
    resource.update("b", 100);
    EXPECT_VALUE(sum, 100);
    EXPECT_VALUE(sampleAvg, 0);
    EXPECT_VALUE(deltaAvg, 25);

    timeShift.applyAbsoluteShift(std::chrono::seconds(6));
    resource.update("b", 0);
    EXPECT_VALUE(sum, 100);
    EXPECT_VALUE(sampleAvg, 100.0 * 2 / 6);
    EXPECT_VALUE(deltaAvg, 16.6);

    for (int i = 6; i <= 10; ++i)
    {
        timeShift.applyAbsoluteShift(std::chrono::seconds(i));
        EXPECT_VALUE(sum, 100);
        EXPECT_VALUE(sampleAvg, 100.0 * 2 / i);
        EXPECT_VALUE(deltaAvg, 100.0 / i);
    }

    for (int i = 1; i <= 3; ++i)
    {
        timeShift.applyAbsoluteShift(std::chrono::seconds(10 + i));
        EXPECT_VALUE(sum, 100);
        EXPECT_VALUE(sampleAvg, 20);
        EXPECT_VALUE(deltaAvg, 10);
    }

    timeShift.applyAbsoluteShift(std::chrono::seconds(15));
    EXPECT_VALUE(sum, 0);
    EXPECT_VALUE(sampleAvg, 10);
    EXPECT_VALUE(deltaAvg, 0);

    for (int i = 1; i <= 5; ++i)
    {
        timeShift.applyAbsoluteShift(std::chrono::seconds(15 + i));
        EXPECT_VALUE(sum, 0);
        EXPECT_VALUE(sampleAvg, 0);
        EXPECT_VALUE(deltaAvg, 0);
    }
}

TEST_F(MetricsGenerators, Text)
{
    const auto error = parseTemplate("x = %x", monitors);
    EXPECT_EQ(error(), "x = {x IS NOT FOUND}");

    const auto values = parseTemplate("a = %a, b = %b", monitors);
    resource.update("a", 7);
    resource.update("b", 8);
    EXPECT_EQ(values(), "a = 7, b = 8");

    resource.update("a", "hello");
    resource.update("b", "world");
    EXPECT_EQ(values(), "a = hello, b = world");
}

} // namespace nx::vms::utils::metrics::test
