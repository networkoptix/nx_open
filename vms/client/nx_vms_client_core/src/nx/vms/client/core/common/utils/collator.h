// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QCollator>

namespace nx::vms::client::core {

class Collator:
    public QObject,
    public QCollator
{
    Q_OBJECT
    Q_ENUMS(Qt::CaseSensitivity)

    Q_PROPERTY(Qt::CaseSensitivity caseSensitivity READ caseSensitivity WRITE setCaseSensitivity)
    Q_PROPERTY(bool numericMode READ numericMode WRITE setNumericMode)
    Q_PROPERTY(bool ignorePunctuation READ ignorePunctuation WRITE setIgnorePunctuation)
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale)

public:
    Q_INVOKABLE int compare(const QString& left, const QString& right) const
    {
        return QCollator::compare(left, right);
    }
};

} // namespace nx::vms::client::core
