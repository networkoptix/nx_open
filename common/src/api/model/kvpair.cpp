#include "kvpair.h"

QnKvPair::QnKvPair(const QString &name, const QString &value):
    m_name(name), m_value(value) 
{}

QnKvPair::QnKvPair(const QString& name, const int value):
    m_name(name), m_value(QString::number(value))
{}

QnKvPair::QnKvPair(const QString& name, const bool value):
    m_name(name), m_value(value ? QLatin1String("True") : QLatin1String("False"))
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

