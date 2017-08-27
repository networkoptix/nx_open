#include "hanwha_cgi_parameter.h"

#include <nx/utils/log/assert.h>

namespace nx {
namespace mediaserver_core {
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

std::pair<int, int> HanwhaCgiParameter::range() const
{
    return std::make_pair(m_min, m_max);
}

void HanwhaCgiParameter::setRange(const std::pair<int, int> range)
{
    m_min = range.first;
    m_max = range.second;
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

std::set<QString> HanwhaCgiParameter::possibleValues() const
{
    return m_possibleValues;
}

void HanwhaCgiParameter::setPossibleValues(std::set<QString> possibleValues)
{
    m_possibleValues = possibleValues;
}

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
