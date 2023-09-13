// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "date_validator.h"

#include <QtQml/QtQml>
#include <QtWidgets/QDateEdit>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QWidget>

#include <nx/utils/log/assert.h>
#include <nx/vms/time/formatter.h>

namespace nx::vms::client::desktop {
namespace {

static const auto kFormat = getFormatString(nx::vms::time::Format::dd_MM_yyyy);

} // namespace

DateValidator::DateValidator(QObject* parent):
    QValidator(parent),
    m_dateEdit(std::make_unique<QDateEdit>())
{
    m_dateEdit->setDisplayFormat(kFormat);
    if (QLineEdit* lineEdit = m_dateEdit->findChild<QLineEdit*>(); NX_ASSERT(lineEdit))
        m_validator = lineEdit->validator();
}

DateValidator::~DateValidator()
{
}

QValidator::State DateValidator::validate(QString& input, int& pos) const
{
    if (!NX_ASSERT(m_validator))
        return QValidator::Acceptable;

    const QValidator::State result = m_validator->validate(input, pos);
    if (result == QValidator::Acceptable)
    {
        const auto date = dateFromString(input);
        return (m_minimum.isNull() || date >= m_minimum.toDate())
            && (m_maximum.isNull() || date <= m_maximum.toDate())
                ? QValidator::Acceptable
                : QValidator::Intermediate;
    }

    return result;
}

void DateValidator::fixup(QString& input) const
{
    if (NX_ASSERT(m_validator))
        m_validator->fixup(input);
}

QString DateValidator::dateSeparator() const
{
    auto result = std::find_if(kFormat.begin(), kFormat.end(),
        [](const QChar& symbol) { return !symbol.isLetterOrNumber(); });

    return NX_ASSERT(result != kFormat.end()) ? QString(*result) : "";
}

QString DateValidator::fixupString(const QString& input) const
{
    QString result{input};
    fixup(result);
    return result;
}

QDate DateValidator::dateFromString(const QString& date) const
{
    // Year 20xx is expected when parsing short format "yy", but QDate::fromString returns 19xx.
    constexpr int kShortYearFormatBase = 2000;
    constexpr int kShortYearFormatOffset = 100; //< Year shift from 19xx to 20xx.

    auto result = QDate::fromString(date, kFormat);
    const bool isShortYearFormat = !kFormat.contains("yyyy");

    return isShortYearFormat && result.year() < kShortYearFormatBase
        ? result.addYears(kShortYearFormatOffset)
        : result;
}

void DateValidator::registerQmlType()
{
    qmlRegisterType<DateValidator>("nx.vms.client.desktop", 1, 0, "DateValidator");
}

} // namespace nx::vms::client::desktop
