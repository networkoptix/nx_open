#include <gtest/gtest.h>

#include <plugins/resource/hanwha/hanwha_range.h>

using namespace nx::vms::server::plugins;

namespace {

template <
    template<typename> typename ContainerType,
    typename ElementType>
struct TestRange
{
    TestRange() = delete;
    TestRange(
        const ContainerType<ElementType>& mapToRange,
        const Range<double>& mapFromRange,
        double valueToMap,
        ElementType result,
        bool isSignCorrectionEnabled)
        :
        mapToRange(mapToRange),
        mapFromRange(mapFromRange),
        valueToMap(valueToMap),
        result(result),
        isSignCorrectionEnabled(isSignCorrectionEnabled)
    {
    };

    ContainerType<ElementType> mapToRange;
    Range<double> mapFromRange;
    double valueToMap;
    ElementType result;
    bool isSignCorrectionEnabled;
};

static const Range<double> kDefaultRange = {-1.0, 1.0};

} // namespace

TEST(HanwhaRange, floatingRange)
{
    static const std::vector<TestRange<Range, double>> kRanges = {
        {{-10.0, 10.0,}, kDefaultRange, 0.5, 5.0, true},
        {{-20.0, 100.0}, kDefaultRange, -0.75, -15.0, true},
        {{0.0, 120.0}, kDefaultRange, 0.2, 24.0, true},
        {{10.0, 20.0}, kDefaultRange, 0.3, 13.0, true},
        {{-20.0, 0.0}, kDefaultRange, -0.1, -2.0, true},
        {{-30.0, -10.0}, kDefaultRange, -0.7, -24.0, true},

        {{-10.0, 10.0}, kDefaultRange, 0.5, 5.0, false},
        {{-20.0, 100.0}, kDefaultRange, -0.75, -5.0, false},
        {{0.0, 120.0}, kDefaultRange, 0.2, 72.0, false},
        {{10.0, 20.0}, kDefaultRange, 0.3, 16.5, false},
        {{-20.0, 0.0}, kDefaultRange, -0.1, -11.0, false},
        {{-30.0, -10.0}, kDefaultRange, -0.7, -27.0, false},
        {{-100.0, 100.0}, kDefaultRange, 0.8, 80.0, false},
        {{-100.0, 10.0}, kDefaultRange, 0.25, -31.25, false}
    };

    for (const auto& testRange: kRanges)
    {
        HanwhaCgiParameter parameter;
        parameter.setName("Test parameter");
        parameter.setType(HanwhaCgiParameterType::floating);
        parameter.setFloatMin(testRange.mapToRange.first);
        parameter.setFloatMax(testRange.mapToRange.second);

        HanwhaRange range(parameter);
        range.setMappingBoundaries(testRange.mapFromRange);
        range.setSignCorrectionEnabled(testRange.isSignCorrectionEnabled);

        bool success = false;
        const auto mappingResult = range.mapValue(testRange.valueToMap)->toDouble(&success);
        ASSERT_DOUBLE_EQ(testRange.result, mappingResult);
        ASSERT_TRUE(success);
    }
}

TEST(HanwhaRange, integerRange)
{
    static const std::vector<TestRange<Range, int>> kRanges = {
        {{-10, 10,}, kDefaultRange, 0.5, 5, true},
        {{-20, 100}, kDefaultRange, -0.75, -15, true},
        {{0, 120}, kDefaultRange, 0.2, 24, true},
        {{10, 20}, kDefaultRange, 0.3, 13, true},
        {{-20, 0}, kDefaultRange, -0.1, -2, true},
        {{-30, -10}, kDefaultRange, -0.7, -24, true},
        {{-100, 100}, kDefaultRange, 1.0, 100, true},

        {{-10, 10}, kDefaultRange, 0.5, 5, false},
        {{-20, 100}, kDefaultRange, -0.75, -5, false},
        {{0, 120}, kDefaultRange, 0.2, 72, false},
        {{10, 20}, kDefaultRange, 0.3, 17, false},
        {{-20, 0}, kDefaultRange, -0.1, -11, false},
        {{-30, -10}, kDefaultRange, -0.7, -27, false},
        {{-100, 100}, kDefaultRange, 0.8, 80, false},
        {{-100, 10}, kDefaultRange, 0.25, -31, false},
        {{-100, 100}, kDefaultRange, 1.0, 100, false},
    };

    for (const auto& testRange : kRanges)
    {
        HanwhaCgiParameter parameter;
        parameter.setName("Test parameter");
        parameter.setType(HanwhaCgiParameterType::integer);
        parameter.setMin(testRange.mapToRange.first);
        parameter.setMax(testRange.mapToRange.second);

        HanwhaRange range(parameter);
        range.setMappingBoundaries(testRange.mapFromRange);
        range.setSignCorrectionEnabled(testRange.isSignCorrectionEnabled);

        bool success = false;
        const auto mappingResult = range.mapValue(testRange.valueToMap)->toInt(&success);
        ASSERT_EQ(testRange.result, mappingResult);
        ASSERT_TRUE(success);
    }
}

TEST(HanwhaRange, enumerationRange)
{
    static const std::vector<TestRange<QList, QString>> kRanges = {
        {{"-100", "-10", "-1", "1", "10", "100"}, kDefaultRange, 0.5, "10", true},
        {{"0", "10", "20", "30" , "40", "50"}, kDefaultRange, 0.2, "10", true},
        {{"Near", "Far"}, kDefaultRange, 0.5, "Far", true},
        {{"Near", "Far"}, kDefaultRange, -0.7, "Near", true},
        {{"Start", "Middle", "Last"}, kDefaultRange, 0.1, "Middle", true},
        {{"-100", "-10", "-1", "1", "10", "100"}, kDefaultRange, 0.5, "10", false},
        {{"0", "10", "20", "30" , "40", "50", "60", "70"}, kDefaultRange, 0.2, "40", false},
        {{"0", "10", "20", "30" , "40", "50", "60", "70"}, kDefaultRange, 0.26, "50", false},
        {{"0", "10", "20", "30" , "40", "50", "60", "70"}, kDefaultRange, -1.0, "0", false}
    };

    for (const auto& testRange: kRanges)
    {
        HanwhaCgiParameter parameter;
        parameter.setName("Test parameter");
        parameter.setType(HanwhaCgiParameterType::enumeration);
        parameter.setPossibleValues(testRange.mapToRange);

        HanwhaRange range(parameter);
        range.setMappingBoundaries(testRange.mapFromRange);
        range.setSignCorrectionEnabled(testRange.isSignCorrectionEnabled);

        const auto mappingResult = range.mapValue(testRange.valueToMap);
        ASSERT_EQ(testRange.result, *mappingResult);
    }
}