#pragma once

#include <functional>
#include <QtGui/QValidator>

#include <core/resource/resource_fwd.h>

class QnUuid;

namespace Qn {

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

TextValidateFunction defaultIntValidator(int minValue, int maxValue, const QString& errorMessage);
TextValidateFunction defaultEmailValidator(bool allowEmpty = true);
TextValidateFunction defaultNonEmptyValidator(const QString& errorMessage);
TextValidateFunction defaultPasswordValidator(bool allowEmpty, const QString& emptyPasswordMessage = QString());
TextValidateFunction defaultConfirmationValidator(TextAccessorFunction primaryText, const QString& errorMessage);

using ValidatorsList = QList<TextValidateFunction>;
TextValidateFunction validatorsConcatenator(const ValidatorsList& validators);

using UserValidator = std::function<bool (const QnUserResourcePtr&)>;
using RoleValidator = std::function<QValidator::State (const QnUuid&)>;

} // namespace Qn
