// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QDate>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QRegularExpression>
#include <QtCore/QVariant>
#include <QtGui/QValidator>

class QDateEdit;

namespace nx::vms::client::desktop {

class DateValidator: public QValidator
{
    Q_OBJECT
    Q_PROPERTY(QVariant minimum MEMBER m_minimum)
    Q_PROPERTY(QVariant maximum MEMBER m_maximum)
    Q_PROPERTY(QString dateSeparator READ dateSeparator CONSTANT)

public:
    DateValidator(QObject* parent = nullptr);
    ~DateValidator() override;

    virtual QValidator::State validate(QString& input, int& pos) const override;
    virtual void fixup(QString& input) const override;

    QString dateSeparator() const;

    Q_INVOKABLE QString fixupString(const QString& input) const;
    Q_INVOKABLE QDate dateFromString(const QString& date) const;

    static void registerQmlType();

private:
    std::unique_ptr<QDateEdit> m_dateEdit; //< Use the validator from QDateEdit.
    QPointer<const QValidator> m_validator;
    QVariant m_minimum;
    QVariant m_maximum;
};

} // namespace nx::vms::client::desktop
