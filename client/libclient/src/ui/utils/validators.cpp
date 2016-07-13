#include "validators.h"

#include <utils/email/email.h>

class QnValidatorStrings
{
    Q_DECLARE_TR_FUNCTIONS(QnValidatorStrings)
};

namespace Qn
{

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

    TextValidateFunction defaultEmailValidator()
    {
        return [](const QString& text)
        {
            if (!text.trimmed().isEmpty() && !QnEmailAddress::isValid(text))
                return Qn::ValidationResult(QnValidatorStrings::tr("E-Mail is not valid."));
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
}
