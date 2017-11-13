#include "validators.h"

#include <QtCore/QCoreApplication>

#include <utils/email/email.h>

class QnValidatorStrings
{
    Q_DECLARE_TR_FUNCTIONS(QnValidatorStrings)
};

namespace Qn {

    ValidationResult::ValidationResult() :
        state(QValidator::Invalid),
        errorMessage()
    {
    }

    ValidationResult::ValidationResult(QValidator::State state, const QString& errorMessage) :
        state(state),
        errorMessage(errorMessage)
    {
    }

    ValidationResult::ValidationResult(const QString& errorMessage) :
        state(QValidator::Invalid),
        errorMessage(errorMessage)
    {
    }

    TextValidateFunction defaultEmailValidator(bool allowEmpty)
    {
        return [allowEmpty](const QString& text)
        {
            if (text.trimmed().isEmpty())
            {
                return allowEmpty
                    ? kValidResult
                    : Qn::ValidationResult(QnValidatorStrings::tr("Email cannot be empty."));
            }

            if (!nx::email::isValidAddress(text)) /* isValid() trims it before checking. */
                return Qn::ValidationResult(QnValidatorStrings::tr("Email is not valid."));

            return kValidResult;
        };
    }

    TextValidateFunction defaultNonEmptyValidator(const QString& errorMessage)
    {
        return [errorMessage](const QString& text)
        {
            if (text.trimmed().isEmpty())
                return Qn::ValidationResult(errorMessage);
            return kValidResult;
        };
    }

    TextValidateFunction defaultPasswordValidator(bool allowEmpty, const QString& emptyPasswordMessage)
    {
        return [allowEmpty, emptyPasswordMessage](const QString& text)
        {
            if (text.isEmpty())
            {
                if (allowEmpty)
                    return kValidResult;

                return Qn::ValidationResult(emptyPasswordMessage.isEmpty()
                    ? QnValidatorStrings::tr("Password cannot be empty.")
                    : emptyPasswordMessage);
            }

            if (text != text.trimmed())
            {
                return Qn::ValidationResult(
                    QnValidatorStrings::tr("Avoid leading and trailing spaces."));
            }

            return kValidResult;
        };
    }

    TextValidateFunction defaultConfirmationValidator(TextAccessorFunction primaryText, const QString& errorMessage)
    {
        return [primaryText, errorMessage](const QString& text)
        {
            if (primaryText() != text)
                return Qn::ValidationResult(errorMessage);

            return kValidResult;
        };
    }

    TextValidateFunction validatorsConcatenator(const ValidatorsList& validators)
    {
        return [validators](const QString& text)
        {

            auto result = kValidResult;
            for (const auto& validator: validators)
            {
                const auto tmpResult = validator(text);
                if (tmpResult.state == QValidator::Invalid)
                    return tmpResult;
                if (tmpResult.state < result.state)
                    result = tmpResult;
            }
            return result;
        };
    }

    TextValidateFunction defaultIntValidator(int minValue, int maxValue, const QString& errorMessage)
    {
        return [minValue, maxValue, errorMessage](const QString& text)
            {
                bool ok;
                int value = text.toInt(&ok);
                ok = ok && value >= minValue && value <= maxValue;

                return ok ? kValidResult : Qn::ValidationResult(errorMessage);
            };
    }

} // namespace Qn
