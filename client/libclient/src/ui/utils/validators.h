#pragma once

#include <functional>
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

    using TextValidateFunction = std::function<ValidationResult (const QString&)>;
    using TextAccessorFunction = std::function<QString(void)>;

    TextValidateFunction defaultEmailValidator();
    TextValidateFunction defaultNonEmptyValidator(const QString& errorMessage);
    TextValidateFunction defaultPasswordValidator(bool allowEmpty, const QString& emptyPasswordMessage = QString());
    TextValidateFunction defaultConfirmationValidator(TextAccessorFunction primaryText, const QString& errorMessage);
}
