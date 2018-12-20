#include "hanwha_cgi_parameter.h"
#include "hanwha_utils.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace vms::server {
namespace plugins {

bool HanwhaCgiParameter::isValid() const
{
    if (!isRequestParameter() && !isResponseParameter())
        return false;

    if (name().isEmpty())
        return false;

    auto parameterType = type();
    if (parameterType == HanwhaCgiParameterType::boolean)
    {
        auto falseVal = falseValue();
        auto trueVal = trueValue();

        return falseVal != trueVal
            && !falseVal.isEmpty()
            && !trueVal.isEmpty();
    }

    if (parameterType == HanwhaCgiParameterType::integer)
        return max() >= min();

    if (parameterType == HanwhaCgiParameterType::floating)
        return floatMax() >= floatMin();

    return true;
}

QString HanwhaCgiParameter::name() const
{
    return m_name;
}

void HanwhaCgiParameter::setName(const QString& parameterName)
{
    m_name = parameterName;
}

HanwhaCgiParameterType HanwhaCgiParameter::type() const
{
    return m_type;
}

void HanwhaCgiParameter::setType(HanwhaCgiParameterType parameterType)
{
    m_type = parameterType;
}

bool HanwhaCgiParameter::isRequestParameter() const
{
    return m_isRequestParameter;
}

void HanwhaCgiParameter::setIsRequestParameter(bool isRequestParameter)
{
    m_isRequestParameter = isRequestParameter;
}

bool HanwhaCgiParameter::isResponseParameter() const
{
    return m_isResponseParameter;
}

void HanwhaCgiParameter::setIsResponseParameter(bool isResponseParameter)
{
    m_isResponseParameter = isResponseParameter;
}

int HanwhaCgiParameter::min() const
{
    return m_min;
}

void HanwhaCgiParameter::setMin(int min)
{
    m_min = min;
}

int HanwhaCgiParameter::max() const
{
    return m_max;
}

void HanwhaCgiParameter::setMax(int max)
{
    m_max = max;
}

float HanwhaCgiParameter::floatMin() const
{
    return m_floatMin;
}

void HanwhaCgiParameter::setFloatMin(float floatMin)
{
    m_floatMin = floatMin;
}

float HanwhaCgiParameter::floatMax() const
{
    return m_floatMax;
}

void HanwhaCgiParameter::setFloatMax(float floatMax)
{
    m_floatMax = floatMax;
}

std::pair<int, int> HanwhaCgiParameter::range() const
{
    return std::make_pair(m_min, m_max);
}

void HanwhaCgiParameter::setRange(const std::pair<int, int> range)
{
    m_min = range.first;
    m_max = range.second;
}

std::pair<float, float> HanwhaCgiParameter::floatRange() const
{
    return std::make_pair(m_floatMin, m_floatMax);
}

void HanwhaCgiParameter::setFloatRange(const std::pair<float, float> range)
{
    m_floatMin = range.first;
    m_floatMax = range.second;
}

QString HanwhaCgiParameter::falseValue() const
{
    return m_falseValue;
}

void HanwhaCgiParameter::setFalseValue(const QString& falseValue)
{
    m_falseValue = falseValue;
}

QString HanwhaCgiParameter::trueValue() const
{
    return m_trueValue;
}

void HanwhaCgiParameter::setTrueValue(const QString& trueValue)
{
    m_trueValue = trueValue;
}

QString HanwhaCgiParameter::formatString() const
{
    return m_formatString;
}

void HanwhaCgiParameter::setFormatString(const QString& formatString)
{
    m_formatString = formatString;
}

QString HanwhaCgiParameter::formatInfo() const
{
    return m_formatInfo;
}

void HanwhaCgiParameter::setFormatInfo(const QString& formatInfo)
{
    m_formatInfo = formatInfo;
}

int HanwhaCgiParameter::maxLength() const
{
    return m_maxLength;
}

void HanwhaCgiParameter::setMaxLength(int maxLength)
{
    m_maxLength = maxLength;
}

QStringList HanwhaCgiParameter::possibleValues() const
{
    return m_possibleValues;
}

void HanwhaCgiParameter::setPossibleValues(QStringList possibleValues)
{
    m_possibleValues = possibleValues;
}

void HanwhaCgiParameter::addPossibleValues(const QString& value)
{
    m_possibleValues.push_back(value);
}

bool HanwhaCgiParameter::isValueSupported(const QString& parameterValue) const
{
    if (m_type == HanwhaCgiParameterType::boolean && toBool(parameterValue) != boost::none)
        return true;

    return m_possibleValues.contains(parameterValue);
}

} // namespace plugins
} // namespace vms::server
} // namespace nx
