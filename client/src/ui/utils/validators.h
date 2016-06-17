#pragma once

#include <QtGui/QValidator>

namespace Qn
{
    struct ValidationResult
    {
        QValidator::State state;
        QString errorMessage;

        ValidationResult();
        ValidationResult(const QString& errorMessage);
        ValidationResult(QValidator::State state, const QString& errorMessage = QString());
    };

    static const ValidationResult kValidResult(QValidator::Acceptable);

    typedef std::function<ValidationResult (const QString&)> TextValidateFunction;
    TextValidateFunction defaultEmailValidator();
    TextValidateFunction defaultNonEmptyValidator(const QString& errorMessage);

}