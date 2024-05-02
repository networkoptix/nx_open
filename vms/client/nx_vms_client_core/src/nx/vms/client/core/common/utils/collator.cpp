// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "collator.h"

namespace nx::vms::client::core {

Collator::Collator(QObject* parent):
    QObject(parent)
{
}

QLocale Collator::locale() const
{
    return m_collator.locale();
}

void Collator::setLocale(const QLocale& value)
{
    if (locale() == value)
        return;

    m_collator.setLocale(value);
    emit changed();
}

Qt::CaseSensitivity Collator::caseSensitivity() const
{
    return m_collator.caseSensitivity();
}

void Collator::setCaseSensitivity(Qt::CaseSensitivity value)
{
    if (caseSensitivity() == value)
        return;

    m_collator.setCaseSensitivity(value);
    emit changed();
}

bool Collator::numericMode() const
{
    return m_collator.numericMode();
}

void Collator::setNumericMode(bool value)
{
    if (numericMode() == value)
        return;

    m_collator.setNumericMode(value);
    emit changed();
}

bool Collator::ignorePunctuation() const
{
    return m_collator.ignorePunctuation();
}

void Collator::setIgnorePunctuation(bool value)
{
    if (ignorePunctuation() == value)
        return;

    m_collator.setIgnorePunctuation(value);
    emit changed();
}

int Collator::compare(const QString& left, const QString& right) const
{
    return m_collator.compare(left, right);
}

} // namespace nx::vms::client::core
