// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QJsonArray>

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
            monitors[id]->setOptional(true);
        }
    }

protected:
    TestResource resource;
    ValueMonitors monitors;

private:
    std::map<QString, Getter<TestResource>> m_getters;
    std::map<QString, Watch<TestResource>> m_watches;
};

TEST_F(MetricsGenerators, RuleSyntaxErrors)
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
        catch(const RuleSyntaxError& error)
        {
            EXPECT_EQ(QString(error.what()), QString(expectedError)) << formula.toStdString();
        }
    }
}

TEST_F(MetricsGenerators, ValueErrors)
{
    const auto plusAB = parseFormulaOrThrow("add %a %b", monitors).generator;
    const auto andAB = parseFormulaOrThrow("and %a %b", monitors).generator;
    const auto resolutionGreaterOrEqualThanAB =
        parseFormulaOrThrow("resolutionGreaterOrEqualThan %a %b", monitors).generator;

    resource.update("a", "zorz");
    resource.update("b", 7);

    EXPECT_THROW(plusAB(), FormulaCalculationError);
    EXPECT_THROW(andAB(), FormulaCalculationError);
    EXPECT_THROW(resolutionGreaterOrEqualThanAB(), FormulaCalculationError);

    resource.update("a", "7x7");
    resource.update("b", "7x7");
    EXPECT_NO_THROW(resolutionGreaterOrEqualThanAB());

    resource.update("b", "7x7.1");
    EXPECT_THROW(resolutionGreaterOrEqualThanAB(), FormulaCalculationError);
}

TEST_F(MetricsGenerators, AlarmErrors)
{
    static const char* kAlarmText = "I am an alarm text!";
    static const char* kErrorMessage = "I am an example error!";
    static const char* kNullErrorMessage = "I am completely null!";

    std::exception_ptr errorToThrow = nullptr;
    const auto valueGenerator = [&errorToThrow]()
    {
        if (errorToThrow != nullptr) std::rethrow_exception(errorToThrow);
        return api::metrics::Value(true);
    };

    AlarmMonitor monitor(
        "id",
        /*isOptional*/ false,
        nx::vms::api::metrics::AlarmLevel::error,
        {valueGenerator, Scope::local},
        [](){ return kAlarmText; });

    const auto alarm = monitor.alarm();
    EXPECT_EQ(alarm->level, nx::vms::api::metrics::AlarmLevel::error);
    EXPECT_EQ(alarm->text, kAlarmText);

    errorToThrow = std::make_exception_ptr(BaseError(kErrorMessage));
    EXPECT_DEATH(monitor.alarm(), kErrorMessage);

    errorToThrow = std::make_exception_ptr(NullValueError(kNullErrorMessage));
    EXPECT_DEATH(monitor.alarm(), kNullErrorMessage);

    errorToThrow = std::make_exception_ptr(NullValueError(kNullErrorMessage));
    AlarmMonitor monitor2(
        "id",
        /*isOptional*/ true,
        nx::vms::api::metrics::AlarmLevel::error,
        {valueGenerator, Scope::local},
        [](){ return kAlarmText; });
    EXPECT_EQ(monitor2.alarm(), std::nullopt);
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

    EXPECT_THROW(plusAB(), NullValueError);
    EXPECT_THROW(minusAB(), NullValueError);
    EXPECT_THROW(equalAB(), NullValueError);
    EXPECT_THROW(notEqualAB(), NullValueError);
    EXPECT_THROW(greaterAB(), NullValueError);

    resource.update("a", 7);
    EXPECT_THROW(plusAB(), NullValueError);
    EXPECT_THROW(minusAB(), NullValueError);
    EXPECT_THROW(equalAB(), NullValueError);
    EXPECT_THROW(notEqualAB(), NullValueError);
    EXPECT_THROW(greaterAB(), NullValueError);

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
    nx::utils::test::ScopedSyntheticMonotonicTime timeShift;
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

    timeShift.applyAbsoluteShift(std::chrono::minutes(56));
    resource.update("a", api::metrics::Value());

    EXPECT_EQ(count(), api::metrics::Value(0));
    EXPECT_EQ(count7(), api::metrics::Value(0));
    EXPECT_EQ(countHello(), api::metrics::Value(1));

    timeShift.applyAbsoluteShift(std::chrono::minutes(60));
    resource.update("a", 7);

    EXPECT_EQ(count(), api::metrics::Value(1));
    EXPECT_EQ(count7(), api::metrics::Value(1));
    EXPECT_EQ(countHello(), api::metrics::Value(1));
}

#define EXPECT_VALUE(GETTER, VALUE) \
{ \
    const auto value = GETTER; \
    const auto number = value.toDouble(); \
    EXPECT_TRUE((number > ((VALUE) - 0.1)) && (number < ((VALUE) + 0.1))) \
        << "Value: " << value.toVariant().toString().toStdString() << std::endl; \
}

TEST_F(MetricsGenerators, ValueFloat)
{
    nx::utils::test::ScopedSyntheticMonotonicTime timeShift;
    const auto sampleAvg = parseFormulaOrThrow("sampleAvg %b 1m", monitors).generator;
    const auto counterDelta = parseFormulaOrThrow("counterDelta %b 40s", monitors).generator;
    const auto counterToAvg = parseFormulaOrThrow("counterToAvg %b 20s", monitors).generator;
    const auto history = parseFormulaOrThrow("history %b 1m", monitors).generator;

    resource.update("b", 0);

    NX_DEBUG(this, history());
    EXPECT_EQ(sampleAvg(), api::metrics::Value());
    EXPECT_EQ(counterDelta(), api::metrics::Value(0));
    EXPECT_EQ(counterToAvg(), api::metrics::Value());

    timeShift.applyAbsoluteShift(std::chrono::seconds(10) + std::chrono::milliseconds(1));
    resource.update("b", 100);

    NX_DEBUG(this, history());
    EXPECT_VALUE(sampleAvg(), 0);
    EXPECT_VALUE(counterDelta(), 100);
    EXPECT_VALUE(counterToAvg(), 10);

    timeShift.applyAbsoluteShift(std::chrono::seconds(20) + std::chrono::milliseconds(2));
    resource.update("b", 200);

    NX_DEBUG(this, history());
    EXPECT_VALUE(sampleAvg(), 50);
    EXPECT_VALUE(counterDelta(), 200);
    EXPECT_VALUE(counterToAvg(), 10);

    timeShift.applyAbsoluteShift(std::chrono::seconds(40) + std::chrono::milliseconds(3));
    resource.update("b", 600);

    NX_DEBUG(this, history());
    EXPECT_VALUE(sampleAvg(), 125);
    EXPECT_VALUE(counterDelta(), 600);
    EXPECT_VALUE(counterToAvg(), 20);

    timeShift.applyAbsoluteShift(std::chrono::minutes(1) + std::chrono::milliseconds(4));
    resource.update("b", 30);

    NX_DEBUG(this, history());
    EXPECT_VALUE(sampleAvg(), 283.3);
    EXPECT_VALUE(counterDelta(), 30 - 200);
    EXPECT_VALUE(counterToAvg(), -28.5);

    timeShift.applyAbsoluteShift(std::chrono::seconds(90) + std::chrono::milliseconds(5));

    NX_DEBUG(this, history());
    EXPECT_VALUE(sampleAvg(), 248.3);
    EXPECT_VALUE(counterDelta(), 30 - 600);
    EXPECT_VALUE(counterToAvg(), 0);

    timeShift.applyAbsoluteShift(std::chrono::minutes(2) + std::chrono::milliseconds(6));

    NX_DEBUG(this, history());
    EXPECT_VALUE(sampleAvg(), 30);
    EXPECT_VALUE(counterDelta(), 0);
    EXPECT_VALUE(counterToAvg(), 0);
}

TEST_F(MetricsGenerators, ValueSpike)
{
    nx::utils::test::ScopedSyntheticMonotonicTime timeShift;
    const auto sampleAvg = parseFormulaOrThrow("sampleAvg %b 10s", monitors).generator;
    const auto counterDelta = parseFormulaOrThrow("counterDelta %b 10s", monitors).generator;
    const auto counterToAvg = parseFormulaOrThrow("counterToAvg %b 10s", monitors).generator;

    resource.update("b", 0);

    EXPECT_EQ(sampleAvg(), api::metrics::Value());
    EXPECT_EQ(counterDelta(), api::metrics::Value(0));
    EXPECT_EQ(counterToAvg(), api::metrics::Value());

    for (int i = 1; i <= 4; ++i)
    {
        timeShift.applyAbsoluteShift(std::chrono::seconds(i));
        resource.update("b", 0);

        EXPECT_EQ(sampleAvg(), 0);
        EXPECT_EQ(counterDelta(), 0);
        EXPECT_EQ(counterToAvg(), 0);
    }

    timeShift.applyAbsoluteShift(std::chrono::seconds(4));
    resource.update("b", 100);

    EXPECT_VALUE(sampleAvg(), 0);
    EXPECT_VALUE(counterDelta(), 100);
    EXPECT_VALUE(counterToAvg(), 25);

    timeShift.applyAbsoluteShift(std::chrono::seconds(6));
    resource.update("b", 0);

    EXPECT_VALUE(sampleAvg(), 100.0 * 2 / 6);
    EXPECT_VALUE(counterDelta(), 0);
    EXPECT_VALUE(counterToAvg(), 0);

    for (int i = 6; i <= 10; ++i)
    {
        timeShift.applyAbsoluteShift(std::chrono::seconds(i));

        EXPECT_VALUE(sampleAvg(), 100.0 * 2 / i);
        EXPECT_VALUE(counterDelta(), 0); // ??
        EXPECT_VALUE(counterToAvg(), 0);
    }

    for (int i = 1; i <= 3; ++i)
    {
        timeShift.applyAbsoluteShift(std::chrono::seconds(10 + i));

        EXPECT_VALUE(sampleAvg(), 20);
        EXPECT_VALUE(counterDelta(), 0);
        EXPECT_VALUE(counterToAvg(), 0);
    }

    timeShift.applyAbsoluteShift(std::chrono::seconds(15));

    EXPECT_VALUE(sampleAvg(), 10);
    EXPECT_VALUE(counterDelta(), -100);
    EXPECT_VALUE(counterToAvg(), -10);

    for (int i = 1; i <= 5; ++i)
    {
        timeShift.applyAbsoluteShift(std::chrono::seconds(15 + i) + std::chrono::milliseconds(1));

        EXPECT_VALUE(sampleAvg(), 0);
        EXPECT_VALUE(counterDelta(), 0);
        EXPECT_VALUE(counterToAvg(), 0);
    }
}

TEST_F(MetricsGenerators, ValueBlanks)
{
    nx::utils::test::ScopedSyntheticMonotonicTime timeShift;
    const auto sampleAvg = parseFormulaOrThrow("sampleAvg %b 1m", monitors).generator;
    const auto counterDelta = parseFormulaOrThrow("counterDelta %b 10s", monitors).generator;
    const auto counterToAvg = parseFormulaOrThrow("counterToAvg %b 20s", monitors).generator;

    EXPECT_EQ(sampleAvg(), api::metrics::Value());
    EXPECT_EQ(counterDelta(), api::metrics::Value());
    EXPECT_EQ(counterToAvg(), api::metrics::Value());

    timeShift.applyAbsoluteShift(std::chrono::seconds(10));
    resource.update("b", 100);

    EXPECT_EQ(sampleAvg(), api::metrics::Value());
    EXPECT_VALUE(counterDelta(), 0);
    EXPECT_VALUE(counterToAvg(), 0);

    timeShift.applyAbsoluteShift(std::chrono::seconds(20));

    EXPECT_VALUE(sampleAvg(), 100);
    EXPECT_VALUE(counterDelta(), 0);
    EXPECT_VALUE(counterToAvg(), 0);

    resource.update("b", api::metrics::Value());

    EXPECT_EQ(sampleAvg(), api::metrics::Value());
    EXPECT_VALUE(counterDelta(), 0);
    EXPECT_VALUE(counterToAvg(), 0);

    timeShift.applyAbsoluteShift(std::chrono::seconds(30) + std::chrono::milliseconds(1));

    EXPECT_EQ(sampleAvg(), api::metrics::Value());
    EXPECT_VALUE(counterDelta(), 0);
    EXPECT_VALUE(counterToAvg(), 0);

    resource.update("b", 200);

    EXPECT_EQ(sampleAvg(), 100);
    EXPECT_VALUE(counterDelta(), 0);
    EXPECT_VALUE(counterToAvg(), 5);

    timeShift.applyAbsoluteShift(std::chrono::seconds(40) + std::chrono::milliseconds(1));

    EXPECT_EQ(sampleAvg(), 150);
    EXPECT_VALUE(counterDelta(), 0);
    EXPECT_VALUE(counterToAvg(), 0);
}

TEST_F(MetricsGenerators, Text)
{
    const auto error = parseTemplate("x = %x", monitors);
    EXPECT_DEATH(error(), "is not found");

    const auto values = parseTemplate("a = %a, b = %b", monitors);
    resource.update("a", 7);
    resource.update("b", 8);
    EXPECT_EQ(values(), "a = 7, b = 8");

    resource.update("a", "hello");
    resource.update("b", "world");
    EXPECT_EQ(values(), "a = hello, b = world");
}

} // namespace nx::vms::utils::metrics::test
