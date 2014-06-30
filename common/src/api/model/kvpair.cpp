#include "kvpair.h"

QnKvPair::QnKvPair():
    m_isPredefinedParam(false)
{}

QnKvPair::QnKvPair(const QString &name, const QString &value, bool _isPredefinedParam):
    m_name(name), m_value(value), m_isPredefinedParam(_isPredefinedParam)
{}

QnKvPair::QnKvPair(const QString& name, const int value, bool _isPredefinedParam):
    m_name(name), m_value(QString::number(value)), m_isPredefinedParam(_isPredefinedParam)
{}

QnKvPair::QnKvPair(const QString& name, const bool value, bool _isPredefinedParam):
    m_name(name), m_value(value ? lit("True") : lit("False")), m_isPredefinedParam(_isPredefinedParam)
{}

void QnKvPair::setName(const QString &name)
{
    m_name = name;
}

const QString &QnKvPair::name() const
{
    return m_name;
}

void QnKvPair::setValue(const QString &value)
{
    m_value = value;
}

const QString &QnKvPair::value() const
{
    return m_value;
}

bool QnKvPair::isPredefinedParam() const
{
    return m_isPredefinedParam;
}
