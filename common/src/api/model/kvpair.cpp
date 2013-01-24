#include "kvpair.h"

QnKvPair::QnKvPair(const QString &name, const QString &value):
    m_name(name), m_value(value) {}

void QnKvPair::setName(const char* name)
{
    m_name = QString::fromUtf8(name);
}

void QnKvPair::setName(const QString &name)
{
    m_name = name;
}

const QString &QnKvPair::name() const
{
    return m_name;
}

void QnKvPair::setValue(const char* value)
{
    m_value = QString::fromUtf8(value);
}

void QnKvPair::setValue(const QString &value)
{
    m_value = value;
}

const QString &QnKvPair::value() const
{
    return m_value;
}

#include "kvpair.h"

