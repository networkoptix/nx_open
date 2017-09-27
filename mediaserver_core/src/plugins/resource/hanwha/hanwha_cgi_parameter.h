#pragma once

#if defined(ENABLE_HANWHA)

#include <set>

#include <QtCore/QString>

#include <nx/utils/literal.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {

enum class HanwhaCgiParameterType
{
    unknown,
    boolean,
    integer,
    floating,
    enumeration,
    string
};

class HanwhaCgiParameter
{
public:
    explicit HanwhaCgiParameter() = default;

    bool isValid() const;

    QString name() const;
    void setName(const QString& name);

    HanwhaCgiParameterType type() const;
    void setType(HanwhaCgiParameterType parameterType);

    bool isRequestParameter() const;
    void setIsRequestParameter(bool isRequestParameter);

    bool isResponseParameter() const;
    void setIsResponseParameter(bool isResponseParameter);

    int min() const;
    void setMin(int min);

    int max() const;
    void setMax(int max);

    // TODO: #dmishin it looks really weird. Get rid of the float* methods
    float floatMin() const;
    void setFloatMin(float floatMin);

    float floatMax() const;
    void setFloatMax(float floatMax);

    std::pair<int, int> range() const;
    void setRange(const std::pair<int, int> range);

    std::pair<float, float> floatRange() const;
    void setFloatRange(const std::pair<float, float> range);

    QString falseValue() const;
    void setFalseValue(const QString& falseValue);

    QString trueValue() const;
    void setTrueValue(const QString& trueValue);

    QString formatString() const;
    void setFormatString(const QString& formatString);

    QString formatInfo() const;
    void setFormatInfo(const QString& formatInfo);

    int maxLength() const;
    void setMaxLength(int maxLength);

    QStringList possibleValues() const;
    void setPossibleValues(QStringList possibleValues);

private:
    QString m_name;
    HanwhaCgiParameterType m_type = HanwhaCgiParameterType::unknown;

    bool m_isResponseParameter = false;
    bool m_isRequestParameter = false;

    int m_min = 0;
    int m_max = 0;

    float m_floatMin = 0;
    float m_floatMax = 0;

    QString m_falseValue = lit("False");
    QString m_trueValue = lit("True");

    QString m_formatString;
    QString m_formatInfo;
    int m_maxLength = 0;

    QStringList m_possibleValues;
};

} // namespace plugins
} // namespace mediaserver_core
} // namespace nx

#endif // defined(ENABLE_HANWHA)

