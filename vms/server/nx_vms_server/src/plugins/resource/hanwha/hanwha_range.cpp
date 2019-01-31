#include "hanwha_range.h"

#include <algorithm>

#include <nx/utils/std/optional.h>
#include <nx/utils/math/arithmetic.h>

namespace nx {
namespace vms::server {
namespace plugins {

namespace {

template<typename T>
nx::utils::math::Sign sign(T number)
{
    if (qFuzzyIsNull(number))
        return nx::utils::math::Sign::zero;

    return nx::utils::math::sign(number);
}

template<typename T>
std::optional<QString> toString(const std::optional<T>& value)
{
    if (value == std::nullopt)
        return std::nullopt;

    return QString::number(value.value());
}

template<typename T>
bool isInRange(double value, const Range<T>& range)
{
    return value >= range.first && value <= range.second;
}

template <typename T>
T rangeLength(const Range<T>& range)
{
    return range.second - range.first;
}

} // namespace

HanwhaRange::HanwhaRange(const HanwhaCgiParameter& rangeParameter):
    m_parameter(rangeParameter)
{
    calculateRangeMap();
}

std::optional<QString> HanwhaRange::mapValue(double value) const
{
    if (value < m_mappingMin || value > m_mappingMax)
    {
        NX_ASSERT(false, lm("Wrong input value %1").arg(value));
        return std::nullopt;
    }

    const auto parameterType = m_parameter.type();
    if (parameterType == HanwhaCgiParameterType::enumeration)
        return mapEnumerationParameter(value);

    if (parameterType == HanwhaCgiParameterType::integer)
        return toString(mapIntegerParameter(value));

    if (parameterType == HanwhaCgiParameterType::floating)
        return toString(mapFloatingParameter(value));

    NX_ASSERT(
        false,
        lm("Wrong parameter type for parameter %1").arg(m_parameter.name()));

    return std::nullopt;
}

void HanwhaRange::setMappingBoundaries(double min, double max)
{
    if (max <= min)
    {
        NX_ASSERT(
            false,
            lm("Wrong mapping range: min - %1, max - %2")
                .args(min, max));

        return;
    }

    m_mappingMin = min;
    m_mappingMax = max;

    calculateRangeMap();
}

void HanwhaRange::setMappingBoundaries(const Range<double>& boundaries)
{
    if (boundaries.second <= boundaries.first)
    {
        NX_ASSERT(
            false,
            lm("Wrong mapping range: min - %1, max - %2")
                .args(boundaries.first, boundaries.second));

        return;
    }

    m_mappingMin = boundaries.first;
    m_mappingMax = boundaries.second;

    calculateRangeMap();
}

void HanwhaRange::setSignCorrectionEnabled(bool isSignCorrectionEnabled)
{
    m_isSignCorrectionEnabled = isSignCorrectionEnabled;
    calculateRangeMap();
}

void HanwhaRange::setRangeMap(const RangeMap<double>& associatedRange)
{
    m_rangeMap = associatedRange;
}

std::optional<QString> HanwhaRange::mapEnumerationParameter(double value) const
{
    for (const auto& entry: m_rangeMap)
    {
        const auto& range = entry.second;
        if (isInRange(value, range))
            return entry.first;
    }

    return std::nullopt;
}

std::optional<int> HanwhaRange::mapIntegerParameter(double value) const
{
    const auto parameterMin = m_parameter.min();
    const auto parameterMax = m_parameter.max();

    const auto result = mapFloatingParameterInternal(value, parameterMin, parameterMax);
    if (result == std::nullopt)
        return std::nullopt;

    return std::clamp(
        (int) (result.value() + (result.value() > 0 ? 0.5 :- 0.5)),
        parameterMin,
        parameterMax);
}

std::optional<double> HanwhaRange::mapFloatingParameter(double value) const
{
    const auto parameterMin = m_parameter.floatMin();
    const auto parameterMax = m_parameter.floatMax();

    return mapFloatingParameterInternal(value, parameterMin, parameterMax);
}

std::optional<double> HanwhaRange::mapFloatingParameterInternal(
    double value,
    double parameterMin,
    double parameterMax) const
{
    if (parameterMax < parameterMin)
        return std::nullopt;

    double mapFromRangeLength = 0.0;
    double mapToRangeLength = 0.0;
    double mapFromZero = 0.0;
    double mapToZero = 0.0;

    if (m_isSignCorrectionEnabled)
    {
        // All negatives must stay negatives, all positives must stay positives.
        // If parameter has positive/negative range then input values with other sign
        // must be treated as an error.
        if (value > 0)
        {
            if (parameterMax < 0)
            {
                NX_ASSERT(false, lm("Wrong parameter value: %1").arg(value));
                return std::nullopt;
            }

            mapFromZero = m_mappingMin < 0 ? 0.0 : m_mappingMin;
            mapToZero = parameterMin < 0 ? 0.0 : parameterMin;
            mapFromRangeLength = m_mappingMax - mapFromZero;
            mapToRangeLength = parameterMax - mapToZero;
        }
        else if (value < 0)
        {
            if (parameterMin > 0)
            {
                NX_ASSERT(false, lm("Wrong parameter value: %1").arg(value));
                return std::nullopt;
            }

            mapFromZero = m_mappingMax < 0 ? m_mappingMax : 0.0;
            mapToZero = parameterMax < 0 ? parameterMax : 0.0;
            mapFromRangeLength = mapFromZero - m_mappingMin;
            mapToRangeLength = mapToZero - parameterMin;
        }
        else
        {
            return 0.0;
        }
    }
    else
    {
        mapFromRangeLength = m_mappingMax - m_mappingMin;
        mapToRangeLength = parameterMax - parameterMin;
        mapFromZero = m_mappingMin;
        mapToZero = parameterMin;
    }

    if (mapFromRangeLength <= 0)
    {
        NX_ASSERT(false, lm("Wrong range length"));
        return std::nullopt;
    }

    if (mapToRangeLength == 0)
        return parameterMin;

    return mapToZero + mapToRangeLength / mapFromRangeLength * (value - mapFromZero);
}

void HanwhaRange::calculateRangeMap()
{
    if (m_parameter.type() != HanwhaCgiParameterType::enumeration)
        return; //< No need for the range map.

    m_rangeMap.clear();

    bool result = false;
    if (m_isSignCorrectionEnabled)
    {
        // First time try to caluculate range map for ranges like [-100, -10, -1, 1, 10, 100].
        result = calculateNumericRangeMap();
    }

    if (!result)
        calculateEnumerationRangeMap(); //< For ranges like [Near, Far]
}

bool HanwhaRange::calculateNumericRangeMap()
{
    using namespace nx::utils;

    bool success = false;
    const auto possibleValues = m_parameter.possibleValues();
    std::map<math::Sign, std::vector<QString>> enumOptionsBySign;
    for (const auto& value: possibleValues)
    {
        const auto numericValue = value.toDouble(&success);
        if (!success)
        {
            m_rangeMap.clear();
            return false;
        }

        enumOptionsBySign[sign(numericValue)].push_back(value);
    }

    auto rangeBoundsBySign =
        [this](math::Sign sign)
        {
            switch (sign)
            {
                case math::Sign::negative:
                    return Range<double>(m_mappingMin, 0.0);
                case math::Sign::positive:
                    return Range<double>(0.0, m_mappingMax);
                case math::Sign::zero:
                default:
                    return Range<double>(0.0, 0.0);
            }
        };

    RangeMap<double> rangeMap;
    for (const auto& entry: enumOptionsBySign)
    {
        const auto sign = entry.first;
        if (sign == math::Sign::zero)
            continue;

        const auto& options = entry.second;
        const auto optionsCount = options.size();
        const auto fullRange = rangeBoundsBySign(sign);
        const double step = rangeLength(fullRange) / optionsCount;

        for (auto i = 0; i < optionsCount; ++i)
        {
            const double rangeStart
                = std::clamp(fullRange.first + step * i, fullRange.first, fullRange.second);
            const double rangeEnd
                = std::clamp(rangeStart + step, fullRange.first, fullRange.second);

            rangeMap[options[i]] = {rangeStart, rangeEnd};
        }
    }

    m_rangeMap = std::move(rangeMap);
    return true;
}

bool HanwhaRange::calculateEnumerationRangeMap()
{
    const auto values = m_parameter.possibleValues();
    const auto valuesCount = values.size();
    const auto length = rangeLength(Range<double>(m_mappingMin, m_mappingMax));
    const double step = length / valuesCount;

    for (auto i = 0; i < valuesCount; ++i)
    {
        const double rangeStart
            = std::clamp(m_mappingMin + step * i, m_mappingMin, m_mappingMax);
        const double rangeEnd
            = std::clamp(rangeStart + step, m_mappingMin, m_mappingMax);

        m_rangeMap[values[i]] = {rangeStart, rangeEnd};
    }

    return true;
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
