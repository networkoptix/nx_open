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
    { }

    ValidationResult::ValidationResult(QValidator::State state, const QString& errorMessage) :
        state(state),
        errorMessage(errorMessage)
    { }



    ValidationResult::ValidationResult(const QString& errorMessage) :
        state(QValidator::Invalid),
        errorMessage(errorMessage)
    {}

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

}