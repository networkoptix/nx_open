#pragma once

#include <map>
#include <vector>

#include <plugins/resource/hanwha/hanwha_cgi_parameter.h>

#include <nx/utils/std/optional.h>

namespace nx {
namespace vms::server {
namespace plugins {

template<typename NumericType>
using Range = std::pair<NumericType, NumericType>;

template <typename NumericType>
using RangeMap = std::map<QString, Range<NumericType>>;

class HanwhaRange
{
public:
    HanwhaRange() = default;
    HanwhaRange(const HanwhaCgiParameter& rangeParameter);

    std::optional<QString> mapValue(double value) const;

    void setMappingBoundaries(double min, double max);
    void setMappingBoundaries(const Range<double>& boundaries);
    void setSignCorrectionEnabled(bool signCorrectionEnabled);
    void setRangeMap(const RangeMap<double>& rangeMap);

private:
    std::optional<QString> mapEnumerationParameter(double value) const;
    std::optional<int> mapIntegerParameter(double value) const;
    std::optional<double> mapFloatingParameter(double value) const;
    std::optional<double> mapFloatingParameterInternal(
        double value,
        double parameterMin,
        double parmeterMax) const;

    void calculateRangeMap();
    bool calculateNumericRangeMap();
    bool calculateEnumerationRangeMap();

private:
    HanwhaCgiParameter m_parameter;

    bool m_isSignCorrectionEnabled = true;
    RangeMap<double> m_rangeMap;

    double m_mappingMin = -1.0;
    double m_mappingMax = 1.0;
};

} // namespace plugins
} // namespace vms::server
} // namespace nx
