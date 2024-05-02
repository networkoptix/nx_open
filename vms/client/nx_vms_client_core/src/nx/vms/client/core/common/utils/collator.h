// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QCollator>
#include <QtCore/QObject>

namespace nx::vms::client::core {

/**
 * A wrapper for QCollator to use in QML.
 */
class NX_VMS_CLIENT_CORE_API Collator: public QObject
{
    Q_OBJECT
    Q_ENUMS(Qt::CaseSensitivity)

    Q_PROPERTY(Qt::CaseSensitivity caseSensitivity READ caseSensitivity WRITE setCaseSensitivity
        NOTIFY changed)
    Q_PROPERTY(bool numericMode READ numericMode WRITE setNumericMode NOTIFY changed)
    Q_PROPERTY(bool ignorePunctuation READ ignorePunctuation WRITE setIgnorePunctuation
        NOTIFY changed)
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale NOTIFY changed)

public:
    explicit Collator(QObject* parent = nullptr);

    QLocale locale() const;
    void setLocale(const QLocale& value);

    Qt::CaseSensitivity caseSensitivity() const;
    void setCaseSensitivity(Qt::CaseSensitivity value);

    bool numericMode() const;
    void setNumericMode(bool value);

    bool ignorePunctuation() const;
    void setIgnorePunctuation(bool value);

    Q_INVOKABLE int compare(const QString& left, const QString& right) const;

signals:
    void changed();

protected:
    QCollator m_collator;
};

} // namespace nx::vms::client::core
